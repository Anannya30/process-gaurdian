#include <assert.h>
#include <stdio.h>

#include "process_tree.h"

int main(void) {
    ProcessInfo processes[] = {
        {100, 1, "", 'R', 0, 0, 0, 0, 0, 0.0},
        {101, 100, "", 'R', 0, 0, 0, 0, 0, 0.0},
        {102, 100, "", 'S', 0, 0, 0, 0, 0, 0.0},
    };
    ProcessTreeNode nodes[3];
    ProcessInfo missing[] = {{103, 999, "", 'R', 0, 0, 0, 0, 0, 0.0}};
    ProcessTreeNode missing_nodes[1];
    ProcessInfo cyclic[] = {
        {200, 201, "", 'R', 0, 0, 0, 0, 0, 0.0},
        {201, 200, "", 'R', 0, 0, 0, 0, 0, 0.0}
    };
    ProcessTreeNode cyclic_nodes[2];

    assert(process_tree_build(processes, 3, nodes, 3) == 3);
    assert(nodes[0].pid == 100 && nodes[0].ppid == 1);
    assert(nodes[0].parent_index == -1 && nodes[0].depth == 0);
    assert(nodes[1].parent_index == 0 && nodes[1].depth == 1);
    assert(nodes[2].parent_index == 0 && nodes[2].depth == 1);

    assert(process_tree_build(missing, 1, missing_nodes, 1) == 1);
    assert(missing_nodes[0].parent_index == -1);
    assert(missing_nodes[0].depth == 0);

    assert(process_tree_build(NULL, 0, NULL, 0) == 0);
    assert(process_tree_build(processes, 3, nodes, 2) == -1);
    assert(process_tree_build(processes, 3, NULL, 3) == -1);

    assert(process_tree_build(cyclic, 2, cyclic_nodes, 2) == 2);
    assert(cyclic_nodes[0].depth >= 0 && cyclic_nodes[1].depth >= 0);

    printf("test_process_tree: PASS\n");
    return 0;
}
