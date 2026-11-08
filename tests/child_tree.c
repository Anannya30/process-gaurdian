#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void child_wait(void) {
    for (;;) {
        pause();
    }
}

int main(void) {
    pid_t children[3];
    int created = 0;
    int i;

    printf("PARENT PID=%ld PPID=%ld\n",
           (long)getpid(), (long)getppid());
    fflush(stdout);

    for (i = 0; i < 3; ++i) {
        pid_t child = fork();
        if (child < 0) {
            fprintf(stderr, "fork failed: %s\n", strerror(errno));
            for (i = 0; i < created; ++i) {
                kill(children[i], SIGTERM);
            }
            for (i = 0; i < created; ++i) {
                waitpid(children[i], NULL, 0);
            }
            return 1;
        }
        if (child == 0) {
            printf("CHILD PID=%ld PPID=%ld\n",
                   (long)getpid(), (long)getppid());
            fflush(stdout);
            child_wait();
            _exit(0);
        }
        children[created++] = child;
    }

    {
        struct timespec delay = {0, 100000000L};
        nanosleep(&delay, NULL);
    }
    for (i = 0; i < created; ++i) {
        kill(children[i], SIGTERM);
    }
    for (i = 0; i < created; ++i) {
        waitpid(children[i], NULL, 0);
    }
    return 0;
}
#define _POSIX_C_SOURCE 200809L
