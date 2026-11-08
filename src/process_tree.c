#include "process_tree.h"

#include <stdlib.h>

int process_tree_build(
    const ProcessInfo processes[],
    int process_count,
    ProcessTreeNode nodes[],
    int max_nodes
) {
    int i;

    if (process_count < 0 || max_nodes < 0 ||
        process_count > max_nodes ||
        (process_count > 0 && processes == NULL) ||
        (max_nodes > 0 && nodes == NULL)) {
        return -1;
    }

    for (i = 0; i < process_count; ++i) {
        nodes[i].pid = processes[i].pid;
        nodes[i].ppid = processes[i].ppid;
        nodes[i].parent_index = -1;
        nodes[i].depth = 0;
    }

    for (i = 0; i < process_count; ++i) {
        int parent;
        for (parent = 0; parent < process_count; ++parent) {
            if (nodes[parent].pid == nodes[i].ppid) {
                nodes[i].parent_index = parent;
                break;
            }
        }
    }

    for (i = 0; i < process_count; ++i) {
        int *visited;
        int current = i;
        int depth = 0;
        int cyclic = 0;

        visited = (int *)calloc((size_t)process_count, sizeof(int));
        if (visited == NULL) {
            return -1;
        }

        while (nodes[current].parent_index >= 0 &&
               !visited[current]) {
            int parent = nodes[current].parent_index;

            visited[current] = 1;
            if (visited[parent]) {
                cyclic = 1;
                break;
            }
            current = parent;
            ++depth;
        }

        if (!cyclic) {
            nodes[i].depth = depth;
        } else {
            nodes[i].depth = 0;
        }
        free(visited);
    }

    return process_count;
}
