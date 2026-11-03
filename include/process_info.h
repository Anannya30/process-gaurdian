#ifndef PROCESS_INFO_H
#define PROCESS_INFO_H

#include <sys/types.h>
#include "common.h"

typedef struct {
    pid_t pid;
    pid_t ppid;
    char name[MAX_PROCESS_NAME];
    char state;
    int priority;
    unsigned long long user_ticks;
    unsigned long long kernel_ticks;
    unsigned long long total_cpu_ticks;
    long memory_kb;
    double cpu_usage_percent;
} ProcessInfo;

#endif
