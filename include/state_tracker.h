#ifndef STATE_TRACKER_H
#define STATE_TRACKER_H

#include "process_info.h"

typedef struct {
    pid_t pid;
    char old_state;
    char new_state;
    int exited;
} ProcessStateChange;

int state_tracker_find_changes(
    const ProcessInfo current[],
    int current_count,
    const ProcessInfo previous[],
    int previous_count,
    ProcessStateChange changes[],
    int max_changes
);

#endif
