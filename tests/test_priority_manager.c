#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "priority_manager.h"

int main(void) {
    pid_t pid = fork();
    int original;
    int value;

    if (pid == 0) {
        execl("./tests/sleep_test", "./tests/sleep_test", (char *)NULL);
        _exit(127);
    }
    assert(pid > 0);
    {
        struct timespec delay = {0, 100000000L};
        nanosleep(&delay, NULL);
    }

    assert(priority_manager_get(pid, &original) == 0);
    assert(priority_manager_set(pid, 0) == 0 ||
           priority_manager_set(pid, 5) == 0);
    if (priority_manager_get(pid, &value) == 0) {
        assert(value >= -20 && value <= 19);
    }
    assert(priority_manager_set(pid, -21) == -1);
    assert(priority_manager_set(pid, 20) == -1);
    (void)priority_manager_set(pid, original);

    kill(pid, 15);
    waitpid(pid, NULL, 0);

    printf("test_priority_manager: PASS\n");
    return 0;
}
#define _POSIX_C_SOURCE 200809L
