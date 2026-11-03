#ifndef ANOMALY_DETECTOR_H
#define ANOMALY_DETECTOR_H

#include "process_info.h"
#include "common.h"

typedef struct {
    double cpu_threshold_percent;
    long memory_growth_threshold_kb;
    int required_consecutive_samples;
} AnomalyConfig;

typedef struct {
    pid_t pid;
    int cpu_high_count;
    int memory_growth_count;
    long previous_memory_kb;
    int cpu_anomaly_active;
    int memory_anomaly_active;
} AnomalyTracker;

typedef struct {
    pid_t pid;
    char type[32];
    double measured_value;
    double threshold;
    char message[MAX_EVENT_TEXT];
} AnomalyEvent;

typedef struct {
    AnomalyConfig config;
    AnomalyTracker trackers[MAX_PROCESSES];
    int tracker_count;
} AnomalyDetector;

void anomaly_detector_init(
    AnomalyDetector *detector,
    const AnomalyConfig *config
);

int anomaly_detector_update(
    AnomalyDetector *detector,
    const ProcessInfo processes[],
    int process_count,
    AnomalyEvent events[],
    int max_events
);

#endif
