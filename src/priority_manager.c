#include "priority_manager.h"

#include <errno.h>
#include <stddef.h>
#include <sys/resource.h>

int priority_manager_get(pid_t pid, int *nice_value) {
    int value;

    if (pid <= 0 || nice_value == NULL) {
        errno = EINVAL;
        return -1;
    }

    errno = 0;
    value = getpriority(PRIO_PROCESS, (id_t)pid);
    if (value == -1 && errno != 0) {
        return -1;
    }

    *nice_value = value;
    return 0;
}

int priority_manager_set(pid_t pid, int nice_value) {
    if (pid <= 0 || nice_value < -20 || nice_value > 19) {
        errno = EINVAL;
        return -1;
    }

    return setpriority(PRIO_PROCESS, (id_t)pid, nice_value) == 0 ? 0 : -1;
}
