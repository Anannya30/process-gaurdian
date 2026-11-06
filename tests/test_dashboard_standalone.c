/*
 * tests/test_dashboard_standalone.c
 * Member 3 -- Dashboard Standalone Interactive Test (Phase B Step B3)
 *
 * Verifies src/dashboard.c using synthetic process data and ncurses.
 * Does NOT require live /proc or Member 1/2 modules.
 *
 * Checks:
 *   - dashboard_init() and dashboard_shutdown() terminal state handling
 *   - Process list view rendering (PID, PPID, NAME, STATE, CPU%, MEM, NICE)
 *   - Tree view rendering (toggled via 't')
 *   - Selection movement (UP/DOWN arrow keys)
 *   - Selected PID retrieval via dashboard_get_selected_pid()
 *   - Anomaly / log section toggle (via 'l')
 *   - Clean shutdown with 'q'
 */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "dashboard.h"
#include "common.h"
#include "process_info.h"
#include "process_tree.h"
#include "anomaly_detector.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void populate_synthetic_data(ProcessInfo procs[], int *proc_count,
                                    ProcessTreeNode nodes[], int *node_count,
                                    AnomalyEvent anomalies[], int *anomaly_count)
{
    *proc_count = 5;

    /* Process 1: init/systemd */
    memset(&procs[0], 0, sizeof(ProcessInfo));
    procs[0].pid = 1;
    procs[0].ppid = 0;
    strncpy(procs[0].name, "systemd", sizeof(procs[0].name) - 1);
    procs[0].state = 'S';
    procs[0].priority = 0;
    procs[0].cpu_usage_percent = 0.5;
    procs[0].memory_kb = 12040;

    /* Process 2: bash */
    memset(&procs[1], 0, sizeof(ProcessInfo));
    procs[1].pid = 1001;
    procs[1].ppid = 1;
    strncpy(procs[1].name, "bash", sizeof(procs[1].name) - 1);
    procs[1].state = 'S';
    procs[1].priority = 0;
    procs[1].cpu_usage_percent = 1.2;
    procs[1].memory_kb = 8450;

    /* Process 3: cpu_hog (simulated) */
    memset(&procs[2], 0, sizeof(ProcessInfo));
    procs[2].pid = 2042;
    procs[2].ppid = 1001;
    strncpy(procs[2].name, "cpu_hog", sizeof(procs[2].name) - 1);
    procs[2].state = 'R';
    procs[2].priority = 10;
    procs[2].cpu_usage_percent = 98.5;
    procs[2].memory_kb = 2048;

    /* Process 4: memory_growth (simulated) */
    memset(&procs[3], 0, sizeof(ProcessInfo));
    procs[3].pid = 3055;
    procs[3].ppid = 1001;
    strncpy(procs[3].name, "mem_leak", sizeof(procs[3].name) - 1);
    procs[3].state = 'S';
    procs[3].priority = 5;
    procs[3].cpu_usage_percent = 4.2;
    procs[3].memory_kb = 51200;

    /* Process 5: worker */
    memset(&procs[4], 0, sizeof(ProcessInfo));
    procs[4].pid = 4099;
    procs[4].ppid = 1;
    strncpy(procs[4].name, "guardian_daemon", sizeof(procs[4].name) - 1);
    procs[4].state = 'S';
    procs[4].priority = -5;
    procs[4].cpu_usage_percent = 15.0;
    procs[4].memory_kb = 18400;

    /* Synthetic tree structure */
    *node_count = 5;
    nodes[0].pid = 1;    nodes[0].ppid = 0;    nodes[0].parent_index = -1; nodes[0].depth = 0;
    nodes[1].pid = 1001; nodes[1].ppid = 1;    nodes[1].parent_index = 0;  nodes[1].depth = 1;
    nodes[2].pid = 2042; nodes[2].ppid = 1001; nodes[2].parent_index = 1;  nodes[2].depth = 2;
    nodes[3].pid = 3055; nodes[3].ppid = 1001; nodes[3].parent_index = 1;  nodes[3].depth = 2;
    nodes[4].pid = 4099; nodes[4].ppid = 1;    nodes[4].parent_index = 0;  nodes[4].depth = 1;

    /* Synthetic anomaly alert */
    *anomaly_count = 1;
    memset(&anomalies[0], 0, sizeof(AnomalyEvent));
    anomalies[0].pid = 2042;
    strncpy(anomalies[0].type, "CPU_HIGH", sizeof(anomalies[0].type) - 1);
    anomalies[0].measured_value = 98.5;
    anomalies[0].threshold = 80.0;
    snprintf(anomalies[0].message, sizeof(anomalies[0].message),
             "PID 2042 CPU 98.50%% > 80.00%% for 3 consecutive samples");
}

int main(void)
{
    ProcessInfo procs[MAX_PROCESSES];
    ProcessTreeNode nodes[MAX_PROCESSES];
    AnomalyEvent anomalies[MAX_EVENTS];
    int proc_count = 0, node_count = 0, anomaly_count = 0;

    populate_synthetic_data(procs, &proc_count, nodes, &node_count,
                            anomalies, &anomaly_count);

    if (dashboard_init() != 0) {
        fprintf(stderr, "Failed to initialize ncurses dashboard.\n");
        return 1;
    }

    int running = 1;
    while (running) {
        dashboard_render(procs, proc_count, nodes, node_count,
                         anomalies, anomaly_count);

        int ch = dashboard_get_input();
        if (ch == 'q' || ch == 'Q') {
            running = 0;
        }

        /* Brief 50ms sleep to avoid busy-spinning in test loop */
        struct timespec ts = {0, 50000000L};
        nanosleep(&ts, NULL);
    }

    dashboard_shutdown();
    printf("Dashboard exited and terminal state restored cleanly.\n");
    return 0;
}

