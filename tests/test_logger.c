#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "logger.h"

int main(void) {
    const char *path = "logs/test_member2.csv";
    GuardianEvent event;
    FILE *file;
    char buffer[512];

    memset(&event, 0, sizeof(event));
    event.timestamp = 1700000000;
    event.pid = 1234;
    strcpy(event.event_type, "STATE_CHANGE");
    strcpy(event.message, "normal message");
    event.measured_value = 12.5;
    event.threshold = 10.0;
    strcpy(event.action, "NONE");
    strcpy(event.result, "OK");

    assert(logger_log_event(&event) == -1);
    assert(logger_init(path) == 0);
    assert(logger_log_event(&event) == 0);

    strcpy(event.message, "value, with \"quotes\"\nand newline");
    strcpy(event.result, "logged");
    assert(logger_log_event(&event) == 0);
    logger_close();

    file = fopen(path, "r");
    assert(file != NULL);
    assert(fgets(buffer, sizeof(buffer), file) != NULL);
    assert(strstr(buffer, "timestamp,pid,event_type") != NULL);
    assert(fgets(buffer, sizeof(buffer), file) != NULL);
    fclose(file);

    assert(logger_init("/root/member2/no-directory/test.csv") == -1);
    logger_close();

    remove(path);
    printf("test_logger: PASS\n");
    return 0;
}
