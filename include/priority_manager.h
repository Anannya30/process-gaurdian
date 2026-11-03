#ifndef PRIORITY_MANAGER_H
#define PRIORITY_MANAGER_H

#include <sys/types.h>

int priority_manager_get(pid_t pid, int *nice_value);
int priority_manager_set(pid_t pid, int nice_value);

#endif
