#ifndef RESOURCE_MONITOR_H
#define RESOURCE_MONITOR_H

#include "process_info.h"

/**
 * Update CPU usage percentages for the current snapshot of processes based on a
 * previous snapshot.
 *
 * @param current          Array of current ProcessInfo structures.
 * @param current_count    Number of entries in the current array.
 * @param previous         Array of previous ProcessInfo structures.
 * @param previous_count   Number of entries in the previous array.
 * @param elapsed_seconds  Time elapsed between the two snapshots.
 */
void resource_monitor_update(
    ProcessInfo current[],
    int current_count,
    const ProcessInfo previous[],
    int previous_count,
    double elapsed_seconds);

#endif // RESOURCE_MONITOR_H
