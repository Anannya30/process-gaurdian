#include "proc_reader.h"
#include "common.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/resource.h>

int proc_reader_read_process(pid_t pid, ProcessInfo *process) {
    if (pid <= 0 || process == NULL) {
        return -1;
    }
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }
    char line[4096];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    // Extract the command name within parentheses
    char *open = strchr(line, '(');
    char *close = strrchr(line, ')');
    if (!open || !close || close <= open) {
        return -1;
    }
    size_t name_len = close - open - 1;
    if (name_len >= MAX_PROCESS_NAME) {
        name_len = MAX_PROCESS_NAME - 1;
    }
    memcpy(process->name, open + 1, name_len);
    process->name[name_len] = '\0';

    // Remaining fields after ')'
    const char *rest = close + 2; // skip space after ')'
    char state_char;
    pid_t ppid;
    unsigned long long utime, stime;
    // sscanf to get state, ppid, utime (14th), stime (15th)
    // We need to skip many fields; using %*s for strings and %*d for ints.
    int scanned = sscanf(rest,
        "%c %d %*d %*d %*d %*d %*d %*u %*u %*u %*u %llu %llu",
        &state_char, &ppid, &utime, &stime);
    if (scanned < 4) {
        return -1;
    }
    process->pid = pid;
    process->ppid = ppid;
    process->state = state_char;
    process->user_ticks = utime;
    process->kernel_ticks = stime;
    process->total_cpu_ticks = utime + stime;
    // Priority
    errno = 0;
    int prio = getpriority(PRIO_PROCESS, pid);
    if (errno != 0) {
        process->priority = 0; // default if unavailable
    } else {
        process->priority = prio;
    }
    // Memory from /proc/<pid>/status
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    fp = fopen(path, "r");
    if (fp) {
        char buf[256];
        long mem_kb = -1;
        while (fgets(buf, sizeof(buf), fp)) {
            if (strncmp(buf, "VmRSS:", 7) == 0) {
                // Format: VmRSS:   12345 kB
                sscanf(buf + 7, "%ld", &mem_kb);
                break;
            }
        }
        fclose(fp);
        process->memory_kb = mem_kb;
    } else {
        process->memory_kb = -1;
    }
    process->cpu_usage_percent = 0.0;
    return 0;
}
