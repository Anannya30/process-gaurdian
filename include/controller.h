#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "anomaly_detector.h"

typedef struct {
    int auto_lower_priority;
    int nice_increment;
} ControllerConfig;

int controller_handle_anomaly(
    const AnomalyEvent *anomaly,
    const ControllerConfig *config,
    int *new_nice_value,
    char action_result[128]
);

#endif
