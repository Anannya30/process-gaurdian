#define _POSIX_C_SOURCE 200809L

#include "process_control.h"

#include <errno.h>
#include <signal.h>
#include <unistd.h>

int process_control_stop(pid_t pid) {
    if (pid <= 0) {
        errno = EINVAL;
        return -1;
    }
    return kill(pid, SIGSTOP) == 0 ? 0 : -1;
}

int process_control_resume(pid_t pid) {
    if (pid <= 0) {
        errno = EINVAL;
        return -1;
    }
    return kill(pid, SIGCONT) == 0 ? 0 : -1;
}

int process_control_terminate(pid_t pid) {
    if (pid <= 0) {
        errno = EINVAL;
        return -1;
    }
    return kill(pid, SIGTERM) == 0 ? 0 : -1;
}
