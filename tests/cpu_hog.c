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
    struct sigaction sa;
    sa.sa_handler = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGTERM, &sa, NULL) != 0) {
        return 1;
    }

    printf("PID=%ld\n", (long)getpid());
    fflush(stdout);

    volatile unsigned long long counter = 0;
    while (!stop_requested) {
        counter++;
    }

    return 0;
}
