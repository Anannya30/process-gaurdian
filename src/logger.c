#include "logger.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

static FILE *logger_file = NULL;

static int csv_field_needs_quotes(const char *value) {
    return strchr(value, ',') != NULL ||
           strchr(value, '"') != NULL ||
           strchr(value, '\n') != NULL ||
           strchr(value, '\r') != NULL;
}

static int csv_write_field(const char *value) {
    const char *p;

    if (!csv_field_needs_quotes(value)) {
        return fprintf(logger_file, "%s", value) >= 0 ? 0 : -1;
    }

    if (fputc('"', logger_file) == EOF) {
        return -1;
    }
    for (p = value; *p != '\0'; ++p) {
        if (*p == '"' && fputc('"', logger_file) == EOF) {
            return -1;
        }
        if (fputc((unsigned char)*p, logger_file) == EOF) {
            return -1;
        }
    }
    return fputc('"', logger_file) == EOF ? -1 : 0;
}

int logger_init(const char *file_path) {
    long end_position;

    if (file_path == NULL || file_path[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    logger_close();
    logger_file = fopen(file_path, "a+");
    if (logger_file == NULL) {
        return -1;
    }

    if (fseek(logger_file, 0L, SEEK_END) != 0) {
        logger_close();
        return -1;
    }
    end_position = ftell(logger_file);
    if (end_position < 0) {
        logger_close();
        return -1;
    }

    if (end_position == 0L) {
        if (fprintf(logger_file,
                    "timestamp,pid,event_type,message,measured_value,"
                    "threshold,action,result\n") < 0 ||
            fflush(logger_file) != 0) {
            logger_close();
            return -1;
        }
    }

    return 0;
}

int logger_log_event(const GuardianEvent *event) {
    char timestamp_text[32];
    struct tm *local_time;

    if (logger_file == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (event == NULL) {
        errno = EINVAL;
        return -1;
    }

    local_time = localtime(&event->timestamp);
    if (local_time == NULL ||
        strftime(timestamp_text, sizeof(timestamp_text),
                 "%Y-%m-%dT%H:%M:%S", local_time) == 0) {
        return -1;
    }

    if (csv_write_field(timestamp_text) != 0 || fputc(',', logger_file) == EOF ||
        fprintf(logger_file, "%ld", (long)event->pid) < 0 ||
        fputc(',', logger_file) == EOF ||
        csv_write_field(event->event_type) != 0 ||
        fputc(',', logger_file) == EOF ||
        csv_write_field(event->message) != 0 ||
        fprintf(logger_file, ",%.17g,", event->measured_value) < 0 ||
        fprintf(logger_file, "%.17g,", event->threshold) < 0 ||
        csv_write_field(event->action) != 0 ||
        fputc(',', logger_file) == EOF ||
        csv_write_field(event->result) != 0 ||
        fputc('\n', logger_file) == EOF ||
        fflush(logger_file) != 0) {
        return -1;
    }

    return 0;
}

void logger_close(void) {
    if (logger_file != NULL) {
        fclose(logger_file);
        logger_file = NULL;
    }
}
