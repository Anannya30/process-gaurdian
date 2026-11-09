#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "proc_reader.h"
#include "process_manager.h"
#include "common.h"

int main(void) {
    /* Step B1: Process reader tests */
    ProcessInfo self_proc;
    memset(&self_proc, 0, sizeof(self_proc));

    /* 1. Call proc_reader_read_process(getpid(), &process) */
    assert(proc_reader_read_process(getpid(), &self_proc) == 0);

    /* 2. Verify returned PID equals getpid() */
    assert(self_proc.pid == getpid());

    /* 3. Verify name is null-terminated and non-empty */
    assert(strlen(self_proc.name) > 0);

    /* 4. Verify PPID is non-negative */
    assert(self_proc.ppid >= 0);

    /* 5. Verify state is populated */
    assert(self_proc.state != '\0');

    /* 6. Verify memory is non-negative when VmRSS is available (-1 if unavailable) */
    assert(self_proc.memory_kb >= -1);

    /* 7. Test an invalid PID and confirm the function returns -1 */
    assert(proc_reader_read_process(-1, &self_proc) == -1);
    assert(proc_reader_read_process(0, &self_proc) == -1);
    assert(proc_reader_read_process(getpid(), NULL) == -1);
    assert(proc_reader_read_process(99999999, &self_proc) == -1);

    /* 8. Short-lived process exit test: confirm failure is handled without crash */
    pid_t child = fork();
    if (child == 0) {
        _exit(0);
    }
    assert(child > 0);
    waitpid(child, NULL, 0);

    ProcessInfo exited_proc;
    (void)proc_reader_read_process(child, &exited_proc);

    /* Step B2: Process enumeration tests */
    static ProcessInfo proc_list[MAX_PROCESSES];

    /* 1. Call process_manager_get_all(processes, MAX_PROCESSES) */
    int count = process_manager_get_all(proc_list, MAX_PROCESSES);

    /* 2. Confirm a non-negative count */
    assert(count >= 0);

    /* 3. Confirm each returned PID is positive */
    for (int i = 0; i < count; i++) {
        assert(proc_list[i].pid > 0);
    }

    /* 4. Confirm there are no duplicate PIDs */
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            assert(proc_list[i].pid != proc_list[j].pid);
        }
    }

    /* 5. Call with a small capacity and verify no overflow occurs */
    ProcessInfo small_list[5];
    int small_count = process_manager_get_all(small_list, 5);
    assert(small_count <= 5);
    assert(small_count >= 0);

    /* Invalid arguments */
    assert(process_manager_get_all(NULL, MAX_PROCESSES) == -1);
    assert(process_manager_get_all(proc_list, 0) == -1);
    assert(process_manager_get_all(proc_list, -5) == -1);

    /* 6. Run enumeration repeatedly */
    for (int iter = 0; iter < 3; iter++) {
        int c = process_manager_get_all(proc_list, MAX_PROCESSES);
        assert(c >= 0);
    }

    printf("test_process_manager: PASS\n");
    return 0;
}
