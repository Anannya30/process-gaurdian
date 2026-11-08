#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested = 0;

static void handle_sigterm(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
}

int main(void) {
    struct sigaction action;

    action.sa_handler = handle_sigterm;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction(SIGTERM, &action, NULL) != 0) {
        return 1;
    }

    printf("PID=%ld PPID=%ld\n", (long)getpid(), (long)getppid());
    fflush(stdout);

    while (!stop_requested) {
        pause();
    }
    return 0;
}
#define _POSIX_C_SOURCE 200809L
