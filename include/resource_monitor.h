#ifndef RESOURCE_MONITOR_H
#define RESOURCE_MONITOR_H

#include "process_info.h"

void resource_monitor_update(
    ProcessInfo current[],
    int current_count,
    const ProcessInfo previous[],
    int previous_count,
    double elapsed_seconds
);

#endif
