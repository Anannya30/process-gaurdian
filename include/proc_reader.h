#ifndef PROC_READER_H
#define PROC_READER_H

#include "process_info.h"

int proc_reader_read_process(pid_t pid, ProcessInfo *process);

#endif
