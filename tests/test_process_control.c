#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "process_control.h"

int main(void) {
    pid_t pid = fork();

    if (pid == 0) {
        execl("./tests/sleep_test", "./tests/sleep_test", (char *)NULL);
        _exit(127);
    }
    assert(pid > 0);
    {
        struct timespec delay = {0, 100000000L};
        nanosleep(&delay, NULL);
    }

    assert(process_control_stop(pid) == 0);
    assert(process_control_resume(pid) == 0);
    assert(process_control_terminate(pid) == 0);

    for (;;) {
        int status;
        pid_t waited = waitpid(pid, &status, 0);
        if (waited == pid) {
            break;
        }
        if (waited < 0 && errno == EINTR) {
            continue;
        }
        assert(0);
    }

    assert(process_control_stop(0) == -1);
    assert(process_control_stop(-1) == -1);
    assert(process_control_stop(99999999) == -1);

    printf("test_process_control: PASS\n");
    return 0;
}
#define _POSIX_C_SOURCE 200809L
