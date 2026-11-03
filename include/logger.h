#ifndef LOGGER_H
#define LOGGER_H

#include "common.h"
#include <time.h>
#include <sys/types.h>

typedef struct {
    time_t timestamp;
    pid_t pid;
    char event_type[32];
    char message[MAX_EVENT_TEXT];
    double measured_value;
    double threshold;
    char action[64];
    char result[128];
} GuardianEvent;

int logger_init(const char *file_path);
int logger_log_event(const GuardianEvent *event);
void logger_close(void);

#endif
