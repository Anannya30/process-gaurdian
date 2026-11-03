#ifndef PROCESS_TREE_H
#define PROCESS_TREE_H

#include "process_info.h"

typedef struct {
    pid_t pid;
    pid_t ppid;
    int parent_index;
    int depth;
} ProcessTreeNode;

int process_tree_build(
    const ProcessInfo processes[],
    int process_count,
    ProcessTreeNode nodes[],
    int max_nodes
);

#endif
