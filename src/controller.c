/*
 * src/controller.c
 * Member 3 -- Adaptive Controller
 *
 * Decides how to respond to a confirmed anomaly event.
 *
 * Rules (MASTER_SPEC.md section 5.3 and MEMBER_3.md Phase A Step A3):
 *   - Report-only mode (auto_lower_priority == 0):
 *       Do NOT call any priority API.
 *       Set new_nice_value to -1.
 *       Write a clear report-only message to action_result.
 *       Return 0.
 *   - Auto-lower mode (auto_lower_priority == 1):
 *       Require nice_increment >= 0.
 *       Read current nice value via Member 2's priority_manager_get.
 *       Add nice_increment, cap at 19.
 *       Apply via Member 2's priority_manager_set.
 *       Store the resulting nice value and a success/failure message.
 *       Return 0 on success, -1 on failure.
 *   - Never call process_control_stop, _resume or _terminate.
 *   - Never call the logger directly.  main.c creates the GuardianEvent.
 */

#include "controller.h"
#include "priority_manager.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * controller_handle_anomaly
 *
 * Handles one anomaly event according to the given configuration.
 *
 * Parameters:
 *   anomaly       -- the confirmed anomaly (must not be NULL)
 *   config        -- controller configuration (must not be NULL)
 *   new_nice_value -- receives the resulting nice value, or -1 in
 *                    report-only mode or on failure (must not be NULL)
 *   action_result -- 128-byte buffer for a human-readable action/result
 *                    string (must not be NULL)
 *
 * Returns 0 when the anomaly was handled (including report-only mode).
 * Returns -1 for invalid input or an operation failure.
 * ------------------------------------------------------------------------- */
int controller_handle_anomaly(const AnomalyEvent *anomaly,
                               const ControllerConfig *config,
                               int *new_nice_value,
                               char action_result[128])
{
    /* ---- Validate all pointer arguments ---- */
    if (anomaly == NULL || config == NULL
            || new_nice_value == NULL || action_result == NULL) {
        return -1;
    }

    /* ---- Report-only mode ---- */
    if (config->auto_lower_priority == 0) {
        /*
         * Do not touch the priority API at all.  Write a clear message so
         * main.c can record exactly what happened in the GuardianEvent log.
         */
        *new_nice_value = -1;
        snprintf(action_result, 128,
                 "Report-only: PID %d [%s] anomaly logged; no priority change.",
                 (int)anomaly->pid, anomaly->type);
        return 0;
    }

    /* ---- Auto-lower-priority mode ---- */

    /* Validate nice_increment before touching the priority API. */
    if (config->nice_increment < 0) {
        snprintf(action_result, 128,
                 "Invalid config: nice_increment must be >= 0 (got %d).",
                 config->nice_increment);
        return -1;
    }

    /*
     * Read the current nice value via Member 2's priority_manager_get.
     * This call correctly handles errno for the getpriority() return value
     * of -1, as specified in MASTER_SPEC.md section 5.2.
     */
    int current_nice = 0;
    if (priority_manager_get(anomaly->pid, &current_nice) != 0) {
        snprintf(action_result, 128,
                 "Failed to read priority for PID %d.", (int)anomaly->pid);
        return -1;
    }

    /* Calculate the new nice value and cap at 19 (lowest scheduling priority). */
    int requested_nice = current_nice + config->nice_increment;
    if (requested_nice > 19) {
        requested_nice = 19;
    }

    /* Apply the new nice value via Member 2's priority_manager_set. */
    if (priority_manager_set(anomaly->pid, requested_nice) != 0) {
        snprintf(action_result, 128,
                 "Failed to set priority for PID %d (tried nice=%d).",
                 (int)anomaly->pid, requested_nice);
        return -1;
    }

    /* Success: record the resulting nice value and a clear status message. */
    *new_nice_value = requested_nice;
    snprintf(action_result, 128,
             "PID %d [%s] nice changed: %d -> %d (increment %d).",
             (int)anomaly->pid, anomaly->type,
             current_nice, requested_nice, config->nice_increment);
    return 0;
}
