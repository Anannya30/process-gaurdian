#include "resource_monitor.h"
#include "common.h"
#include <unistd.h>

void resource_monitor_update(
    ProcessInfo current[],
    int current_count,
    const ProcessInfo previous[],
    int previous_count,
    double elapsed_seconds) {
    if (current == NULL || previous == NULL || elapsed_seconds <= 0.0) {
        return;
    }
    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    if (ticks_per_sec <= 0) {
        // Invalid tick rate, set usage to 0 for all
        for (int i = 0; i < current_count; ++i) {
            current[i].cpu_usage_percent = 0.0;
        }
        return;
    }
    for (int i = 0; i < current_count; ++i) {
        const ProcessInfo *cur = &current[i];
        double usage = 0.0;
        // Find matching previous record
        const ProcessInfo *prev = NULL;
        for (int j = 0; j < previous_count; ++j) {
            if (previous[j].pid == cur->pid) {
                prev = &previous[j];
                break;
            }
        }
        if (prev == NULL) {
            usage = 0.0;
        } else if (cur->total_cpu_ticks < prev->total_cpu_ticks) {
            usage = 0.0;
        } else {
            unsigned long long delta_ticks = cur->total_cpu_ticks - prev->total_cpu_ticks;
            double seconds = ((double)delta_ticks) / (double)ticks_per_sec;
            if (seconds <= 0.0) {
                usage = 0.0;
            } else {
                usage = (seconds / elapsed_seconds) * 100.0;
            }
        }
        current[i].cpu_usage_percent = usage;
    }
}
