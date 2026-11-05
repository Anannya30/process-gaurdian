#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include "process_info.h"

/**
 * Populate the provided array with information about all processes currently
 * visible in the /proc filesystem.
 *
 * @param processes   Array of ProcessInfo structures to fill.
 * @param max_count   Maximum number of entries the array can hold.
 * @return Number of process records stored, or -1 on fatal error (e.g.,
 *         unable to open /proc).
 */
int process_manager_get_all(ProcessInfo processes[], int max_count);

#endif // PROCESS_MANAGER_H
