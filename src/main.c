/*
 * src/main.c
 * Member 3 -- Program Entry Point and Integration Layer
 *
 * Composes all Guardian modules into the main monitoring loop.
 *
 * Rules (MASTER_SPEC.md section 6 and MEMBER_3.md Phase A Step A5):
 *   - Set default configuration values exactly as in MASTER_SPEC.md 6.1.
 *   - Use fixed arrays sized by MAX_PROCESSES and MAX_EVENTS.
 *   - Use a monotonic clock for elapsed-time calculations.
 *   - Call module APIs; do NOT duplicate their internal logic here.
 *   - Log every state change, anomaly, and user-requested action.
 *   - Restore terminal and close the logger on all exit paths.
 *
 * main.c does NOT implement process discovery, CPU/memory calculation,
 * tree building, state tracking, signals, priority management or logging
 * -- those belong to Member 1 and Member 2 modules respectively.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ncurses is needed here only for the confirmation/input prompts that
 * must be done inside the monitoring loop while the dashboard is active. */
#include <ncurses.h>

/* Shared headers */
#include "common.h"
#include "process_info.h"

/* Member 1 APIs */
#include "process_manager.h"
#include "resource_monitor.h"

/* Member 2 APIs */
#include "process_tree.h"
#include "state_tracker.h"
#include "process_control.h"
#include "priority_manager.h"
#include "logger.h"

/* Member 3 APIs */
#include "anomaly_detector.h"
#include "controller.h"
#include "dashboard.h"

/* -------------------------------------------------------------------------
 * Default configuration constants (MASTER_SPEC.md section 6.1).
 * These values must NOT be changed without explicit team approval.
 * ------------------------------------------------------------------------- */
#define CFG_CPU_THRESHOLD      80.0    /* Percent of one logical CPU        */
#define CFG_MEM_GROWTH_KB      10240L  /* KiB growth per sample             */
#define CFG_CONSEC_SAMPLES     3       /* Consecutive samples before alert  */
#define CFG_AUTO_LOWER_PRIO    0       /* 0 = report-only (default/safe)    */
#define CFG_NICE_INCREMENT     5       /* Nice-value step when auto-lowering */
#define LOG_PATH               "logs/process_guardian.csv"

/* -------------------------------------------------------------------------
 * sleep_ms
 *
 * Sleeps for the given number of milliseconds using nanosleep.
 * Does not busy-spin.
 * ------------------------------------------------------------------------- */
static void sleep_ms(long ms)
{
    if (ms <= 0) {
        return;
    }
    struct timespec ts;
    ts.tv_sec  =  ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* -------------------------------------------------------------------------
 * timespec_diff_seconds
 *
 * Returns the elapsed time in seconds between two CLOCK_MONOTONIC readings.
 * ------------------------------------------------------------------------- */
static double timespec_diff_seconds(const struct timespec *start,
                                     const struct timespec *end)
{
    double sec  = (double)(end->tv_sec  - start->tv_sec);
    double nsec = (double)(end->tv_nsec - start->tv_nsec) / 1.0e9;
    return sec + nsec;
}

/* -------------------------------------------------------------------------
 * make_state_change_event
 *
 * Converts a ProcessStateChange into a GuardianEvent suitable for logging.
 * ------------------------------------------------------------------------- */
static GuardianEvent make_state_change_event(const ProcessStateChange *change)
{
    GuardianEvent ev;
    memset(&ev, 0, sizeof(ev));

    ev.timestamp = time(NULL);
    ev.pid       = change->pid;

    if (change->exited) {
        strncpy(ev.event_type, "PROCESS_EXIT", sizeof(ev.event_type) - 1);
        snprintf(ev.message, sizeof(ev.message),
                 "PID %d exited.", (int)change->pid);
    } else {
        strncpy(ev.event_type, "STATE_CHANGE", sizeof(ev.event_type) - 1);
        snprintf(ev.message, sizeof(ev.message),
                 "PID %d state changed: '%c' -> '%c'.",
                 (int)change->pid, change->old_state, change->new_state);
    }

    ev.measured_value = 0.0;
    ev.threshold      = 0.0;
    strncpy(ev.action, "none",   sizeof(ev.action) - 1);
    strncpy(ev.result, "logged", sizeof(ev.result) - 1);

    return ev;
}

/* -------------------------------------------------------------------------
 * make_anomaly_log_event
 *
 * Creates a GuardianEvent from an AnomalyEvent plus the controller output.
 * ------------------------------------------------------------------------- */
static GuardianEvent make_anomaly_log_event(const AnomalyEvent *anomaly,
                                             int new_nice,
                                             const char action_result[128])
{
    GuardianEvent ev;
    memset(&ev, 0, sizeof(ev));

    ev.timestamp = time(NULL);
    ev.pid       = anomaly->pid;

    strncpy(ev.event_type, anomaly->type, sizeof(ev.event_type) - 1);
    ev.event_type[sizeof(ev.event_type) - 1] = '\0';

    strncpy(ev.message, anomaly->message, sizeof(ev.message) - 1);
    ev.message[sizeof(ev.message) - 1] = '\0';

    ev.measured_value = anomaly->measured_value;
    ev.threshold      = anomaly->threshold;

    if (new_nice >= 0) {
        snprintf(ev.action, sizeof(ev.action),
                 "lower_priority_to_%d", new_nice);
    } else {
        strncpy(ev.action, "report_only", sizeof(ev.action) - 1);
    }

    strncpy(ev.result, action_result, sizeof(ev.result) - 1);
    ev.result[sizeof(ev.result) - 1] = '\0';

    return ev;
}

/* -------------------------------------------------------------------------
 * log_user_action  (internal helper)
 *
 * Builds and logs a GuardianEvent for a user-requested process operation.
 * ------------------------------------------------------------------------- */
static void log_user_action(pid_t pid,
                              const char *event_type,
                              const char *message,
                              const char *action,
                              const char *result)
{
    GuardianEvent ev;
    memset(&ev, 0, sizeof(ev));

    ev.timestamp = time(NULL);
    ev.pid       = pid;

    strncpy(ev.event_type, event_type, sizeof(ev.event_type) - 1);
    ev.event_type[sizeof(ev.event_type) - 1] = '\0';

    strncpy(ev.message, message, sizeof(ev.message) - 1);
    ev.message[sizeof(ev.message) - 1] = '\0';

    ev.measured_value = 0.0;
    ev.threshold      = 0.0;

    strncpy(ev.action, action, sizeof(ev.action) - 1);
    ev.action[sizeof(ev.action) - 1] = '\0';

    strncpy(ev.result, result, sizeof(ev.result) - 1);
    ev.result[sizeof(ev.result) - 1] = '\0';

    logger_log_event(&ev);
}

/* -------------------------------------------------------------------------
 * handle_key
 *
 * Dispatches a single key code to the appropriate process-control or
 * priority action.  Returns 0 to continue the loop, 1 to exit.
 *
 * Uses ncurses input prompts only for 'k' (confirmation) and 'p' (nice
 * value input).  nodelay is temporarily cleared during blocking reads.
 * ------------------------------------------------------------------------- */
static int handle_key(int ch)
{
    switch (ch) {

    /* ---- q: quit ---- */
    case 'q':
        return 1; /* Signal the caller to exit the main loop. */

    /* ---- s: stop selected process ---- */
    case 's': {
        int pid = dashboard_get_selected_pid();
        if (pid <= 0) break;

        int rc = process_control_stop((pid_t)pid);

        char msg[MAX_EVENT_TEXT];
        snprintf(msg, sizeof(msg),
                 "User requested SIGSTOP for PID %d.", pid);
        log_user_action((pid_t)pid, "USER_STOP", msg,
                        "SIGSTOP",
                        (rc == 0) ? "signal_sent" : "signal_failed");
        break;
    }

    /* ---- c: continue (resume) selected process ---- */
    case 'c': {
        int pid = dashboard_get_selected_pid();
        if (pid <= 0) break;

        int rc = process_control_resume((pid_t)pid);

        char msg[MAX_EVENT_TEXT];
        snprintf(msg, sizeof(msg),
                 "User requested SIGCONT for PID %d.", pid);
        log_user_action((pid_t)pid, "USER_RESUME", msg,
                        "SIGCONT",
                        (rc == 0) ? "signal_sent" : "signal_failed");
        break;
    }

    /* ---- k: request termination (requires confirmation) ---- */
    case 'k': {
        int pid = dashboard_get_selected_pid();
        if (pid <= 0) break;

        /*
         * Display a confirmation prompt on the second-to-last row.
         * Temporarily switch to blocking input so we wait for the answer.
         */
        int prompt_row = getmaxy(stdscr) - 2;
        if (prompt_row < 0) prompt_row = 0;

        mvhline(prompt_row, 0, ' ', getmaxx(stdscr));
        mvprintw(prompt_row, 0,
                 "Terminate PID %d?  Press 'y' to confirm, any other key to cancel.",
                 pid);
        clrtoeol();
        refresh();

        nodelay(stdscr, FALSE); /* Block until user responds */
        int answer = getch();
        nodelay(stdscr, TRUE);  /* Restore non-blocking mode */

        char msg[MAX_EVENT_TEXT];
        if (answer == 'y' || answer == 'Y') {
            int rc = process_control_terminate((pid_t)pid);
            snprintf(msg, sizeof(msg),
                     "User confirmed SIGTERM for PID %d.", pid);
            log_user_action((pid_t)pid, "USER_TERMINATE", msg,
                            "SIGTERM",
                            (rc == 0) ? "signal_sent" : "signal_failed");
        } else {
            snprintf(msg, sizeof(msg),
                     "User cancelled terminate for PID %d.", pid);
            log_user_action((pid_t)pid, "USER_TERMINATE", msg,
                            "none", "cancelled");
        }
        break;
    }

    /* ---- p: request a nice-value change ---- */
    case 'p': {
        int pid = dashboard_get_selected_pid();
        if (pid <= 0) break;

        /*
         * Prompt for a nice value in the range [-20, 19].
         * Echo input so the user can see what they type.
         */
        int prompt_row = getmaxy(stdscr) - 2;
        if (prompt_row < 0) prompt_row = 0;

        mvhline(prompt_row, 0, ' ', getmaxx(stdscr));
        mvprintw(prompt_row, 0,
                 "New nice value for PID %d [-20..19]: ", pid);
        clrtoeol();
        refresh();

        echo();
        nodelay(stdscr, FALSE);
        char nice_buf[8] = "";
        getnstr(nice_buf, (int)sizeof(nice_buf) - 1);
        noecho();
        nodelay(stdscr, TRUE);

        char msg[MAX_EVENT_TEXT];
        snprintf(msg, sizeof(msg),
                 "User requested nice change for PID %d.", pid);

        int requested_nice = 0;
        if (sscanf(nice_buf, "%d", &requested_nice) == 1
                && requested_nice >= -20
                && requested_nice <= 19) {
            int rc = priority_manager_set((pid_t)pid, requested_nice);
            char result_buf[128];
            snprintf(result_buf, sizeof(result_buf),
                     "%s (nice=%d)",
                     (rc == 0) ? "success" : "failed",
                     requested_nice);
            log_user_action((pid_t)pid, "USER_NICE", msg,
                            "setpriority", result_buf);
        } else {
            log_user_action((pid_t)pid, "USER_NICE", msg,
                            "setpriority", "rejected_invalid_value");
        }
        break;
    }

    default:
        /*
         * 't' and 'l' are handled internally by dashboard_get_input().
         * KEY_UP and KEY_DOWN are handled there too.
         * All other keys are silently ignored.
         */
        break;
    }

    return 0; /* Continue the main loop. */
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
    /* ---- Fixed-size arrays (MASTER_SPEC.md section 6.3) ---- */
    static ProcessInfo      current[MAX_PROCESSES];
    static ProcessInfo      previous[MAX_PROCESSES];
    static ProcessTreeNode  tree_nodes[MAX_PROCESSES];
    static ProcessStateChange state_changes[MAX_EVENTS];
    static AnomalyEvent     anomaly_events[MAX_EVENTS];

    int current_count  = 0;
    int previous_count = 0;
    int node_count     = 0;
    int change_count   = 0;
    int anomaly_count  = 0;

    /* ---- Anomaly detector configuration ---- */
    AnomalyConfig anomaly_cfg;
    anomaly_cfg.cpu_threshold_percent        = CFG_CPU_THRESHOLD;
    anomaly_cfg.memory_growth_threshold_kb   = CFG_MEM_GROWTH_KB;
    anomaly_cfg.required_consecutive_samples = CFG_CONSEC_SAMPLES;

    AnomalyDetector detector;
    anomaly_detector_init(&detector, &anomaly_cfg);

    /* ---- Controller configuration ---- */
    ControllerConfig ctrl_cfg;
    ctrl_cfg.auto_lower_priority = CFG_AUTO_LOWER_PRIO;
    ctrl_cfg.nice_increment      = CFG_NICE_INCREMENT;

    /* ---- Step 1: Initialise logger ---- */
    if (logger_init(LOG_PATH) != 0) {
        /*
         * Non-fatal: the application continues but no events will be
         * persisted to disk.
         */
        fprintf(stderr,
                "Warning: could not open log file '%s'. "
                "Events will not be saved.\n",
                LOG_PATH);
    }

    /* ---- Step 2: Initialise ncurses dashboard ---- */
    if (dashboard_init() != 0) {
        fprintf(stderr,
                "Error: failed to initialise the ncurses dashboard.\n");
        logger_close();
        return 1;
    }

    /* ---- Step 3: Collect the initial process snapshot ---- */
    current_count = process_manager_get_all(current, MAX_PROCESSES);
    if (current_count < 0) {
        dashboard_shutdown();
        logger_close();
        fprintf(stderr,
                "Error: failed to enumerate /proc at startup.\n");
        return 1;
    }

    /*
     * Copy the initial snapshot into "previous" so the first loop iteration
     * has a valid baseline.  CPU deltas in that first iteration will be ~0%
     * which is the correct and safe behaviour.
     */
    memcpy(previous, current,
           (size_t)current_count * sizeof(ProcessInfo));
    previous_count = current_count;

    /* ---- Timing baseline ---- */
    struct timespec prev_sample_time, sample_time, iter_start;
    clock_gettime(CLOCK_MONOTONIC, &prev_sample_time);

    /* ---- Step 4: Main monitoring loop ---- */
    int running = 1;
    while (running) {

        /* 1. Record the start of this iteration for sleep budget. */
        clock_gettime(CLOCK_MONOTONIC, &iter_start);

        /* 2. Discover current processes. */
        current_count = process_manager_get_all(current, MAX_PROCESSES);
        if (current_count < 0) {
            /*
             * Enumeration failure: log a warning, skip processing for
             * this cycle, and use an empty current list so previous data
             * is not accidentally fed to the detector as "current".
             */
            current_count = 0;
        }

        /* 3. Record the time of this successful sample collection. */
        clock_gettime(CLOCK_MONOTONIC, &sample_time);
        double elapsed = timespec_diff_seconds(&prev_sample_time, &sample_time);
        if (elapsed <= 0.0) {
            elapsed = 1.0; /* Guard against zero/negative elapsed time. */
        }

        /* 4. Calculate per-process CPU usage percentages. */
        resource_monitor_update(current,  current_count,
                                 previous, previous_count,
                                 elapsed);

        /* 5. Build the process tree. */
        node_count = process_tree_build(current, current_count,
                                         tree_nodes, MAX_PROCESSES);
        if (node_count < 0) {
            node_count = 0;
        }

        /* 6. Detect state changes and process exits. */
        change_count = state_tracker_find_changes(
            current,  current_count,
            previous, previous_count,
            state_changes, MAX_EVENTS);
        if (change_count < 0) {
            change_count = 0;
        }

        /* 7. Convert state changes to GuardianEvent records and log them. */
        for (int i = 0; i < change_count; i++) {
            GuardianEvent ev = make_state_change_event(&state_changes[i]);
            logger_log_event(&ev);
        }

        /* 8. Run the anomaly detector. */
        anomaly_count = anomaly_detector_update(
            &detector, current, current_count,
            anomaly_events, MAX_EVENTS);
        if (anomaly_count < 0) {
            anomaly_count = 0;
        }

        /* 9. For each anomaly, ask the controller what to do, then log it. */
        for (int i = 0; i < anomaly_count; i++) {
            int  new_nice      = -1;
            char action_result[128] = "";

            controller_handle_anomaly(&anomaly_events[i], &ctrl_cfg,
                                       &new_nice, action_result);

            GuardianEvent ev = make_anomaly_log_event(
                &anomaly_events[i], new_nice, action_result);
            logger_log_event(&ev);
        }

        /* 10. Render the dashboard. */
        dashboard_render(current, current_count,
                          tree_nodes, node_count,
                          anomaly_events, anomaly_count);

        /* 11. Read one key without blocking the sampling loop. */
        int ch = dashboard_get_input();

        /* 12. Handle user commands; exit when handle_key returns 1. */
        if (handle_key(ch) != 0) {
            running = 0;
        }

        /* 13. Wait for the remaining portion of the configured interval. */
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double used_ms = timespec_diff_seconds(&iter_start, &now) * 1000.0;
        long remaining_ms = (long)DEFAULT_SAMPLE_INTERVAL_MS - (long)used_ms;
        sleep_ms(remaining_ms);

        /* 14. Advance the sample timestamp and copy current -> previous. */
        prev_sample_time = sample_time;

        if (current_count > 0) {
            memcpy(previous, current,
                   (size_t)current_count * sizeof(ProcessInfo));
        }
        previous_count = current_count;
    }

    /* ---- Cleanup on normal exit ---- */
    dashboard_shutdown();
    logger_close();

    return 0;
}
