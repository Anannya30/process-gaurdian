#include <assert.h>
#include <stdio.h>

#include "state_tracker.h"

int main(void) {
    ProcessInfo previous[] = {
        {10, 1, "", 'S', 0, 0, 0, 0, 0, 0.0},
        {11, 1, "", 'R', 0, 0, 0, 0, 0, 0.0},
        {12, 1, "", 'D', 0, 0, 0, 0, 0, 0.0}
    };
    ProcessInfo current[] = {
        {10, 1, "", 'R', 0, 0, 0, 0, 0, 0.0},
        {11, 1, "", 'R', 0, 0, 0, 0, 0, 0.0},
        {13, 1, "", 'R', 0, 0, 0, 0, 0, 0.0}
    };
    ProcessStateChange changes[4];

    assert(state_tracker_find_changes(
        current, 3, previous, 3, changes, 4) == 2);
    assert(changes[0].pid == 10 && changes[0].old_state == 'S' &&
           changes[0].new_state == 'R' && changes[0].exited == 0);
    assert(changes[1].pid == 12 && changes[1].exited == 1 &&
           changes[1].new_state == '\0');

    assert(state_tracker_find_changes(
        previous, 3, previous, 3, changes, 4) == 0);
    assert(state_tracker_find_changes(
        NULL, 0, NULL, 0, changes, 4) == 0);
    assert(state_tracker_find_changes(
        current, 3, previous, 3, changes, 1) == 2);
    assert(state_tracker_find_changes(
        current, -1, previous, 3, changes, 4) == -1);
    assert(state_tracker_find_changes(
        current, 3, previous, 3, NULL, 4) == -1);

    printf("test_state_tracker: PASS\n");
    return 0;
}
