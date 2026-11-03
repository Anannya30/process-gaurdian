# Member 2 --- Process Tree, State Tracking, Control and Logging

**Branch:** `feature/process-control`\
**Owned implementation:** Process relationships, state changes, signals,
priority and event logging\
**Read first:** `MASTER_SPEC.md` in full

## 1. Your responsibility

Implement the modules that answer:

-   Which process is the parent of another process?
-   How can the current process list be represented as a tree?
-   Which processes changed state or exited between two snapshots?
-   Can the user stop, resume or gracefully terminate a selected
    process?
-   What is the process nice value, and can it be changed?
-   How can detections, state changes and user actions be recorded?

You do not implement process discovery, CPU/memory sampling, anomaly
detection, adaptive policy, dashboard or `main.c`.

## 2. Exact file ownership

You may create/edit:

-   `include/process_tree.h`
-   `include/state_tracker.h`
-   `include/process_control.h`
-   `include/priority_manager.h`
-   `include/logger.h`
-   `src/process_tree.c`
-   `src/state_tracker.c`
-   `src/process_control.c`
-   `src/priority_manager.c`
-   `src/logger.c`
-   `tests/test_process_tree.c`
-   `tests/test_state_tracker.c`
-   `tests/test_process_control.c`
-   `tests/test_priority_manager.c`
-   `tests/test_logger.c`
-   `tests/child_tree.c`
-   `tests/sleep_test.c`

Do not edit any other source/header/test file. Do not change `common.h`
or `process_info.h` unilaterally.

## 3. Required public APIs

Implement exactly these declarations from `MASTER_SPEC.md`:

``` c
int process_tree_build(
    const ProcessInfo processes[],
    int process_count,
    ProcessTreeNode nodes[],
    int max_nodes
);

int state_tracker_find_changes(
    const ProcessInfo current[],
    int current_count,
    const ProcessInfo previous[],
    int previous_count,
    ProcessStateChange changes[],
    int max_changes
);

int process_control_stop(pid_t pid);
int process_control_resume(pid_t pid);
int process_control_terminate(pid_t pid);

int priority_manager_get(pid_t pid, int *nice_value);
int priority_manager_set(pid_t pid, int nice_value);

int logger_init(const char *file_path);
int logger_log_event(const GuardianEvent *event);
void logger_close(void);
```

Do not change names, parameters, return types or semantics.

## 4. Implementation sequence

### Phase A --- Independent implementation

#### Step A1: Inspect repository and shared structures

1.  Read `MASTER_SPEC.md`.
2.  Run `git status`.
3.  Confirm you are on `feature/process-control`.
4.  Inspect `common.h` and `process_info.h`.
5.  Confirm the exact `ProcessInfo` fields.
6.  Do not modify shared headers. If a required field is missing, stop
    and ask.

#### Step A2: Implement process tree

1.  Create `include/process_tree.h` with the exact `ProcessTreeNode`
    structure and function declaration.
2.  Create `src/process_tree.c`.
3.  Validate pointers, counts and capacity.
4.  Copy each supplied process's PID and PPID into one output node.
5.  Find each node's parent by matching its PPID to another node's PID.
6.  Set `parent_index` to the parent's output-array index; set `-1` when
    the parent is not in the supplied list.
7.  Determine depth from the parent relationship.
8.  Treat a node with no available parent as depth zero.
9.  Detect malformed cycles and prevent infinite traversal.
10. Return the number of nodes, or `-1` for invalid arguments.
11. Do not sort or mutate the input `ProcessInfo` array.

#### Step A3: Implement state tracking

1.  Create `include/state_tracker.h` with the exact `ProcessStateChange`
    structure and function declaration.
2.  Create `src/state_tracker.c`.
3.  Validate arrays, counts and capacity.
4.  For each PID in the current snapshot, search the previous snapshot.
5.  If the PID exists in both and the state character differs, append a
    state-change record.
6.  If the PID exists in current but not previous, do not report a state
    change.
7.  For each PID in previous but not current, append an exit record with
    `exited = 1`.
8.  For ordinary changes, set `exited = 0`, `old_state` to the previous
    state and `new_state` to the current state.
9.  Do not assume an absent PID was intentionally terminated; report it
    as no longer present.
10. Return the number of changes or `-1` for invalid arguments.
11. Never write beyond `max_changes`.

#### Step A4: Implement process control

1.  Create `include/process_control.h`.
2.  Create `src/process_control.c`.
3.  Validate that PID is positive.
4.  Implement stop using `kill(pid, SIGSTOP)`.
5.  Implement resume using `kill(pid, SIGCONT)`.
6.  Implement terminate using `kill(pid, SIGTERM)`.
7.  Return `0` when the signal call succeeds and `-1` when it fails.
8.  Preserve OS errors through `errno`; do not hide permission or
    missing-process errors.
9.  Do not retry with SIGKILL.
10. Do not claim a process has changed state merely because `kill()`
    succeeded; state must be observed in a later sample.

#### Step A5: Implement priority manager

1.  Create `include/priority_manager.h`.
2.  Create `src/priority_manager.c`.
3.  `priority_manager_get` validates PID and output pointer.
4.  Set `errno = 0` before calling `getpriority`.
5.  If the returned nice value is `-1` and errno is nonzero, report
    failure.
6.  `priority_manager_set` validates PID and nice value.
7.  Reject nice values below -20 or above 19.
8.  Call `setpriority(PRIO_PROCESS, pid, nice_value)`.
9.  Return `0` on success and `-1` on failure.
10. Do not bypass OS permissions or request elevated privileges
    automatically.

#### Step A6: Implement event logger

1.  Create `include/logger.h` with the exact `GuardianEvent` structure
    and APIs.
2.  Create `src/logger.c`.
3.  Keep one private file pointer inside this module; do not expose it.
4.  `logger_init(file_path)` opens the file in append mode.
5.  If the file is empty/new, write one CSV header row.
6.  `logger_log_event` validates that the logger is initialized and
    event is non-null.
7.  Convert the timestamp to a readable local timestamp string.
8.  Write timestamp, PID, event type, message, measured value,
    threshold, action and result.
9.  Escape CSV text fields correctly: quote fields containing comma,
    quote or newline, and double embedded quote characters.
10. Check write/flush errors and return `-1` on failure.
11. `logger_close` closes the file if initialized and resets the private
    pointer.
12. Do not add a separate logging library.

#### Step A7: Implement controlled test programs

`tests/child_tree.c`: 1. Parent prints its PID. 2. Parent creates
exactly three children with `fork()`. 3. Each child prints PID and PPID.
4. Children remain alive in a controlled wait loop. 5. Parent reaps
children with `wait()` or `waitpid()`. 6. Handle fork failure and
terminate/reap already-created children safely.

`tests/sleep_test.c`: 1. Print PID and PPID. 2. Wait in a loop until
SIGTERM is received. 3. Do not ignore SIGTERM. 4. Keep the program
otherwise idle so it is safe to test stop/resume.

### Phase B --- Independent testing

#### Step B1: Process tree tests

1.  Build a small synthetic `ProcessInfo` array with one parent and
    multiple children.
2.  Call `process_tree_build`.
3.  Verify each PID and PPID is preserved.
4.  Verify child `parent_index` points to the correct parent node.
5.  Verify root depth is zero and child depth is greater than parent
    depth.
6.  Test a missing parent.
7.  Test empty input and capacity boundaries.
8.  Test malformed cyclic relationships and confirm no infinite loop.

#### Step B2: State tracker tests

1.  Build previous/current snapshots with the same PID and same state;
    expect no event.
2.  Change one state; expect one non-exit change.
3.  Remove a previous PID from current; expect one exit event.
4.  Add a new PID only to current; expect no state-change event.
5.  Test multiple simultaneous changes.
6.  Test output capacity and invalid arguments.

#### Step B3: Process-control tests

1.  Compile `tests/sleep_test.c`.
2.  Start it and record its PID.
3.  Call `process_control_stop(pid)` and verify success.
4.  Confirm the process state becomes stopped in a later observation.
5.  Call `process_control_resume(pid)` and confirm it resumes.
6.  Call `process_control_terminate(pid)`.
7.  Confirm the process exits.
8.  Test invalid and already-exited PIDs.
9.  Never use a system process for these tests.

#### Step B4: Priority tests

1.  Start an owned `sleep_test` or another owned test process.
2.  Read its nice value using `priority_manager_get`.
3.  Set a permitted test value, normally a non-negative nice value.
4.  Read it again and verify the change.
5.  Test values below -20 and above 19; both must be rejected.
6.  Handle permission errors without treating them as code failures.
7.  Restore the test process's original nice value if the OS permits it;
    otherwise terminate the test process.

#### Step B5: Logger tests

1.  Initialize a temporary test CSV file.
2.  Verify the header exists.
3.  Write one normal event.
4.  Write an event whose message contains commas and quotes.
5.  Close and reopen the file.
6.  Verify records were appended and CSV fields remain correctly
    escaped.
7.  Test invalid path and logging before initialization.
8.  Remove only the temporary test file created by the test.

#### Step B6: Build checks

Compile your modules with:

`gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude -c src/process_tree.c src/state_tracker.c src/process_control.c src/priority_manager.c src/logger.c`

Fix all warnings caused by your code. Do not link the final application
yet.

### Phase C --- Integration

1.  Pull the latest `main` after your pull request is approved/merged
    according to the team merge order.
2.  Do not edit Member 1 or Member 3 implementation files.
3.  Confirm all public declarations match `MASTER_SPEC.md`.
4.  Confirm Member 3 can call tree, state, process-control, priority and
    logger APIs.
5.  Confirm Member 1's `ProcessInfo` layout is unchanged.
6.  Run the full build when Member 3 has integrated the application.
7.  Test the tree using Member 2's `child_tree` program.
8.  Test state tracking and process control using `sleep_test`.
9.  Test priority changes only on an owned test process.
10. Inspect the CSV log after the integrated demo.
11. If an API mismatch exists, stop and report it; do not silently
    change the API.
12. Report commands and actual test results to the team.

## 5. Completion checklist

-   [ ] All five module headers and source files exist.
-   [ ] Exact APIs and structures implemented.
-   [ ] Tree links and depths are correct.
-   [ ] Cycles cannot cause infinite traversal.
-   [ ] State changes and exits are detected.
-   [ ] Stop/resume/SIGTERM work on owned test processes.
-   [ ] Priority get/set handles errno and permissions.
-   [ ] CSV logger appends valid escaped records.
-   [ ] Test programs compile and work.
-   [ ] Module tests pass.
-   [ ] No files outside your ownership were modified.
-   [ ] Changes pushed on `feature/process-control` with a pull request.
