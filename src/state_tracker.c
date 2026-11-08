#include "state_tracker.h"

#include <stddef.h>

int state_tracker_find_changes(
    const ProcessInfo current[],
    int current_count,
    const ProcessInfo previous[],
    int previous_count,
    ProcessStateChange changes[],
    int max_changes
) {
    int i;
    int change_count = 0;

    if (current_count < 0 || previous_count < 0 || max_changes < 0 ||
        (current_count > 0 && current == NULL) ||
        (previous_count > 0 && previous == NULL) ||
        (max_changes > 0 && changes == NULL)) {
        return -1;
    }

    for (i = 0; i < current_count; ++i) {
        int previous_index;
        for (previous_index = 0; previous_index < previous_count;
             ++previous_index) {
            if (previous[previous_index].pid == current[i].pid) {
                if (previous[previous_index].state != current[i].state) {
                    if (change_count < max_changes) {
                        changes[change_count].pid = current[i].pid;
                        changes[change_count].old_state =
                            previous[previous_index].state;
                        changes[change_count].new_state = current[i].state;
                        changes[change_count].exited = 0;
                    }
                    ++change_count;
                }
                break;
            }
        }
    }

    for (i = 0; i < previous_count; ++i) {
        int current_index;
        int found = 0;
        for (current_index = 0; current_index < current_count;
             ++current_index) {
            if (current[current_index].pid == previous[i].pid) {
                found = 1;
                break;
            }
        }
        if (!found) {
            if (change_count < max_changes) {
                changes[change_count].pid = previous[i].pid;
                changes[change_count].old_state = previous[i].state;
                changes[change_count].new_state = '\0';
                changes[change_count].exited = 1;
            }
            ++change_count;
        }
    }

    return change_count;
}
