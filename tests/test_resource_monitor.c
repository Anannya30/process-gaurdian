#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "resource_monitor.h"

int main(void) {
    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    assert(ticks_per_sec > 0);

    /* Step B3: CPU sampling tests */
    /* 1. Create two ProcessInfo arrays with the same PID */
    ProcessInfo prev[2];
    ProcessInfo curr[2];
    memset(prev, 0, sizeof(prev));
    memset(curr, 0, sizeof(curr));

    prev[0].pid = 1234;
    prev[0].total_cpu_ticks = 100;

    curr[0].pid = 1234;
    curr[0].total_cpu_ticks = 150;

    /* 2 & 3 & 4. Compare result against the formula using floating-point tolerance */
    double elapsed = 1.0;
    resource_monitor_update(curr, 1, prev, 1, elapsed);

    double expected = ((150.0 - 100.0) / (double)ticks_per_sec) / elapsed * 100.0;
    assert(fabs(curr[0].cpu_usage_percent - expected) < 0.001);

    /* 5. Test a newly discovered PID (not in previous snapshot) */
    curr[1].pid = 5678;
    curr[1].total_cpu_ticks = 200;
    resource_monitor_update(curr, 2, prev, 1, elapsed);
    assert(curr[1].cpu_usage_percent == 0.0);

    /* 6. Test invalid elapsed time */
    curr[0].cpu_usage_percent = 99.0;
    resource_monitor_update(curr, 1, prev, 1, 0.0);
    resource_monitor_update(curr, 1, prev, 1, -1.0);

    /* 7. Test decreasing CPU counters (cur < prev) */
    curr[0].total_cpu_ticks = 50; /* lower than prev 100 */
    resource_monitor_update(curr, 1, prev, 1, elapsed);
    assert(curr[0].cpu_usage_percent == 0.0);

    /* Test NULL pointers and zero counts safely */
    resource_monitor_update(NULL, 0, prev, 1, elapsed);
    resource_monitor_update(curr, 1, NULL, 0, elapsed);

    printf("test_resource_monitor: PASS\n");
    return 0;
}
