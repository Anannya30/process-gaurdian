#ifndef PROC_READER_H
#define PROC_READER_H

#include "process_info.h"

/**
 * Read process information for a single PID.
 *
 * @param pid      Process ID to read (must be > 0).
 * @param process  Pointer to a ProcessInfo structure that will be filled.
 * @return 0 on success, -1 on failure (invalid arguments, file errors, parse errors).
 */
int proc_reader_read_process(pid_t pid, ProcessInfo *process);

#endif // PROC_READER_H
