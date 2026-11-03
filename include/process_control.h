#ifndef PROCESS_CONTROL_H
#define PROCESS_CONTROL_H

#include <sys/types.h>

int process_control_stop(pid_t pid);
int process_control_resume(pid_t pid);
int process_control_terminate(pid_t pid);

#endif
