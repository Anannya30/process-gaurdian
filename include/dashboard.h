#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "process_info.h"
#include "process_tree.h"
#include "anomaly_detector.h"
#include "logger.h"

int dashboard_init(void);
void dashboard_shutdown(void);

void dashboard_render(
    const ProcessInfo processes[],
    int process_count,
    const ProcessTreeNode nodes[],
    int node_count,
    const AnomalyEvent anomalies[],
    int anomaly_count
);

int dashboard_get_input(void);
int dashboard_get_selected_pid(void);

#endif
