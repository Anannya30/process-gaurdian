/*
 * src/anomaly_detector.c
 * Member 3 -- Anomaly Detection Module
 *
 * Detects persistent CPU and memory-growth anomalies across a process list.
 *
 * Rules (MASTER_SPEC.md section 5.3 and MEMBER_3.md Phase A Step A2):
 *   - CPU anomaly:    cpu_usage_percent strictly > cpu_threshold_percent
 *                     for required_consecutive_samples consecutive updates.
 *   - Memory anomaly: memory_kb increase >= memory_growth_threshold_kb
 *                     for required_consecutive_samples consecutive updates.
 *   - Emit each anomaly type ONCE when it first becomes confirmed; do not
 *     re-emit while the condition remains continuously active.
 *   - Clear the active flag when the condition is not met so the anomaly
 *     can be emitted again after recovery and recurrence.
 *   - Remove trackers for PIDs that are no longer in the process list.
 *   - Never call the logger, controller, dashboard, signals or priority APIs.
 */

#include "anomaly_detector.h"

#include <string.h>
#include <stdio.h>

/* -------------------------------------------------------------------------
 * anomaly_detector_init
 *
 * Copies the supplied configuration into the detector, zeros the tracker
 * array, and marks every previous_memory_kb slot as -1 (no prior reading).
 * ------------------------------------------------------------------------- */
void anomaly_detector_init(AnomalyDetector *detector,
                            const AnomalyConfig *config)
{
    if (detector == NULL || config == NULL) {
        return;
    }

    detector->config        = *config;
    detector->tracker_count = 0;

    /* Zero all tracker slots so stale data cannot mislead later checks. */
    memset(detector->trackers, 0, sizeof(detector->trackers));

    /*
     * -1 signals "no prior memory reading" for a tracker.
     * A growth comparison requires at least one previous reading, so the
     * very first sample for each PID sets the baseline without triggering
     * a growth event.
     */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        detector->trackers[i].previous_memory_kb = -1;
    }
}

/* -------------------------------------------------------------------------
 * find_tracker  (internal)
 *
 * Returns the index of the tracker for pid, or -1 if none exists.
 * ------------------------------------------------------------------------- */
static int find_tracker(const AnomalyDetector *detector, pid_t pid)
{
    for (int i = 0; i < detector->tracker_count; i++) {
        if (detector->trackers[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

/* -------------------------------------------------------------------------
 * alloc_tracker  (internal)
 *
 * Appends a fresh, zeroed tracker for pid at the end of the active slice.
 * Returns the new index, or -1 when the array is already full.
 * ------------------------------------------------------------------------- */
static int alloc_tracker(AnomalyDetector *detector, pid_t pid)
{
    if (detector->tracker_count >= MAX_PROCESSES) {
        return -1; /* Tracker array is full; cannot track this PID. */
    }

    int idx = detector->tracker_count;
    detector->tracker_count++;

    AnomalyTracker *t = &detector->trackers[idx];
    memset(t, 0, sizeof(*t));
    t->pid               = pid;
    t->previous_memory_kb = -1; /* No prior reading yet. */

    return idx;
}

/* -------------------------------------------------------------------------
 * anomaly_detector_update
 *
 * Evaluates every process in the current snapshot and updates per-PID
 * trackers.  Emits at most max_events anomaly events.
 *
 * Returns the number of events generated, or -1 for invalid arguments.
 * ------------------------------------------------------------------------- */
int anomaly_detector_update(AnomalyDetector *detector,
                             const ProcessInfo processes[],
                             int process_count,
                             AnomalyEvent events[],
                             int max_events)
{
    /* ---- Validate arguments ---- */
    if (detector == NULL || processes == NULL || events == NULL) {
        return -1;
    }
    if (process_count < 0 || max_events < 0) {
        return -1;
    }
    /*
     * required_consecutive_samples must be a positive integer; a value of
     * zero or less would trigger anomalies on the very first sample, which
     * contradicts the persistence rule.
     */
    if (detector->config.required_consecutive_samples <= 0) {
        return -1;
    }

    int event_count = 0;

    /* ================================================================== */
    /* Phase 1: iterate the current process list and update each tracker. */
    /* ================================================================== */
    for (int i = 0; i < process_count; i++) {
        const ProcessInfo *proc = &processes[i];

        /* Find or allocate a tracker for this PID. */
        int tidx = find_tracker(detector, proc->pid);
        if (tidx < 0) {
            tidx = alloc_tracker(detector, proc->pid);
            if (tidx < 0) {
                /* Tracker array full; skip tracking this PID. */
                continue;
            }
        }

        AnomalyTracker *tracker = &detector->trackers[tidx];

        /* ---------------------------------------------------------------- */
        /* CPU anomaly check                                                  */
        /* ---------------------------------------------------------------- */
        if (proc->cpu_usage_percent > detector->config.cpu_threshold_percent) {
            tracker->cpu_high_count++;
        } else {
            /*
             * Condition not met: reset counter and clear the active flag so
             * the anomaly can be emitted again if the condition recurs later.
             */
            tracker->cpu_high_count    = 0;
            tracker->cpu_anomaly_active = 0;
        }

        /*
         * Emit a CPU event exactly once when the persistence threshold is
         * first reached.  The active flag prevents repeated emission while
         * the condition stays continuously true.
         */
        if (tracker->cpu_high_count
                >= detector->config.required_consecutive_samples
                && tracker->cpu_anomaly_active == 0
                && event_count < max_events) {

            AnomalyEvent *ev = &events[event_count++];
            ev->pid = proc->pid;

            /* Type string is exactly "CPU_HIGH" per spec. */
            strncpy(ev->type, "CPU_HIGH", sizeof(ev->type) - 1);
            ev->type[sizeof(ev->type) - 1] = '\0';

            ev->measured_value = proc->cpu_usage_percent;
            ev->threshold      = detector->config.cpu_threshold_percent;

            snprintf(ev->message, sizeof(ev->message),
                     "PID %d CPU %.2f%% > %.2f%% for %d consecutive samples",
                     (int)proc->pid,
                     proc->cpu_usage_percent,
                     detector->config.cpu_threshold_percent,
                     detector->config.required_consecutive_samples);

            tracker->cpu_anomaly_active = 1;
        }

        /* ---------------------------------------------------------------- */
        /* Memory-growth anomaly check                                        */
        /* ---------------------------------------------------------------- */
        if (tracker->previous_memory_kb >= 0) {
            /*
             * previous_memory_kb is valid: compare growth against the
             * configured threshold.
             */
            long growth = proc->memory_kb - tracker->previous_memory_kb;
            if (growth >= detector->config.memory_growth_threshold_kb) {
                tracker->memory_growth_count++;
            } else {
                tracker->memory_growth_count    = 0;
                tracker->memory_anomaly_active  = 0;
            }
        }
        /*
         * If previous_memory_kb is -1 this is the first sample for this PID.
         * Leave memory_growth_count at zero and do not emit an event;
         * the next sample will start the comparison.
         */

        /*
         * Update the baseline AFTER the growth check so the comparison is
         * always current-vs-previous, not current-vs-current.
         */
        tracker->previous_memory_kb = proc->memory_kb;

        if (tracker->memory_growth_count
                >= detector->config.required_consecutive_samples
                && tracker->memory_anomaly_active == 0
                && event_count < max_events) {

            AnomalyEvent *ev = &events[event_count++];
            ev->pid = proc->pid;

            strncpy(ev->type, "MEMORY_GROWTH", sizeof(ev->type) - 1);
            ev->type[sizeof(ev->type) - 1] = '\0';

            ev->measured_value = (double)proc->memory_kb;
            ev->threshold      = (double)detector->config.memory_growth_threshold_kb;

            snprintf(ev->message, sizeof(ev->message),
                     "PID %d memory grew >= %ld KiB for %d consecutive samples",
                     (int)proc->pid,
                     detector->config.memory_growth_threshold_kb,
                     detector->config.required_consecutive_samples);

            tracker->memory_anomaly_active = 1;
        }
    }

    /* ================================================================== */
    /* Phase 2: remove trackers for PIDs absent from the current list.    */
    /*                                                                      */
    /* Compact the tracker array in-place: keep a tracker only if its PID  */
    /* still appears in the current process snapshot.  This prevents the   */
    /* fixed-size array from filling up with stale entries over time.      */
    /* ================================================================== */
    int new_count = 0;
    for (int i = 0; i < detector->tracker_count; i++) {
        int found = 0;
        for (int j = 0; j < process_count; j++) {
            if (detector->trackers[i].pid == processes[j].pid) {
                found = 1;
                break;
            }
        }
        if (found) {
            if (new_count != i) {
                detector->trackers[new_count] = detector->trackers[i];
            }
            new_count++;
        }
    }
    detector->tracker_count = new_count;

    return event_count;
}
