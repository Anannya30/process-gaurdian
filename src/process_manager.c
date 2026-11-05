#include "process_manager.h"
#include "proc_reader.h"
#include <dirent.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

int process_manager_get_all(ProcessInfo processes[], int max_count) {
    if (processes == NULL || max_count <= 0) {
        return -1;
    }
    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) {
        return -1;
    }
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(proc_dir)) != NULL) {
        // directory name must be all digits
        const char *name = entry->d_name;
        int is_digit = 1;
        for (const char *p = name; *p; ++p) {
            if (!isdigit((unsigned char)*p)) {
                is_digit = 0;
                break;
            }
        }
        if (!is_digit) continue;
        pid_t pid = (pid_t)atoi(name);
        if (pid <= 0) continue;
        ProcessInfo pi;
        if (proc_reader_read_process(pid, &pi) == 0) {
            if (count < max_count) {
                processes[count++] = pi;
            } else {
                // reached capacity, stop adding but continue reading to consume entries
                break;
            }
        }
        // on failure simply skip
    }
    closedir(proc_dir);
    return count;
}
