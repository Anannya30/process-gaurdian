#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CHUNK_SIZE (1024 * 1024) /* 1 MiB */
#define MAX_CHUNKS 64            /* Stop at 64 MiB */

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

    char *chunks[MAX_CHUNKS] = {0};
    int chunk_count = 0;
    struct timespec delay = {0, 100000000L}; /* 100 ms */

    while (!stop_requested) {
        if (chunk_count < MAX_CHUNKS) {
            char *ptr = malloc(CHUNK_SIZE);
            if (ptr == NULL) {
                fprintf(stderr, "Allocation failed at %d MiB\n", chunk_count);
                break;
            }
            /* Touch allocated pages so they are accounted in VmRSS */
            memset(ptr, 0xAA, CHUNK_SIZE);
            chunks[chunk_count++] = ptr;
        }

        nanosleep(&delay, NULL);
    }

    /* Free memory on graceful exit */
    for (int i = 0; i < chunk_count; i++) {
        free(chunks[i]);
    }

    return 0;
}
