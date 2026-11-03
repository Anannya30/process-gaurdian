# Member 1 --- Process Discovery and Resource Monitoring

**Branch:** `feature/monitoring`\
**Owned implementation:** Process discovery, `/proc` parsing, CPU and
memory sampling\
**Read first:** `MASTER_SPEC.md` in full

## 1. Your responsibility

Implement the modules that answer:

-   Which processes currently exist?
-   What are their PID, PPID, name, state, priority, CPU counters and
    resident memory?
-   How much CPU has each process used between two samples?
-   What memory amount is currently resident?

You own only the files listed below. Do not implement process tree
construction, state-transition comparison, process control, logging,
anomaly rules, controller, dashboard or `main.c`.

## 2. Exact file ownership

You may create/edit:

-   `include/process_manager.h`
-   `include/proc_reader.h`
-   `include/resource_monitor.h`
-   `src/process_manager.c`
-   `src/proc_reader.c`
-   `src/resource_monitor.c`
-   `tests/test_process_manager.c`
-   `tests/test_resource_monitor.c`
-   `tests/cpu_hog.c`
-   `tests/memory_growth.c`

Do not edit any other source/header/test file. Shared headers `common.h`
and `process_info.h` are not yours to change unilaterally.

## 3. Required public APIs

Implement exactly these declarations from `MASTER_SPEC.md`:

``` c
int proc_reader_read_process(pid_t pid, ProcessInfo *process);

int process_manager_get_all(ProcessInfo processes[], int max_count);

void resource_monitor_update(
    ProcessInfo current[],
    int current_count,
    const ProcessInfo previous[],
    int previous_count,
    double elapsed_seconds
);
```

Do not change names, parameters, return types or semantics.

## 4. Implementation sequence

### Phase A --- Independent implementation

#### Step A1: Inspect the repository

1.  Read `MASTER_SPEC.md`.
2.  Run `git status`.
3.  Confirm you are on `feature/monitoring`.
4.  Inspect the shared `include/common.h` and `include/process_info.h`.
5.  Do not edit shared headers. If a required field is missing, stop and
    ask.

#### Step A2: Implement `proc_reader_read_process`

1.  Create `include/proc_reader.h` with the exact declaration and
    include guard.
2.  Create `src/proc_reader.c`.
3.  Validate `pid > 0` and `process != NULL`.
4.  Build `/proc/<pid>/stat` path using `snprintf`.
5.  Open the file and read its line with a bounded buffer.
6.  Parse the command name carefully. The `comm` field is enclosed in
    parentheses and can contain spaces and parentheses; do not split the
    entire line with a naive whitespace `strtok` approach.
7.  Extract PID, state, PPID, user ticks and kernel ticks using the
    correct Linux `/proc/[pid]/stat` field positions.
8.  Set `total_cpu_ticks = user_ticks + kernel_ticks`.
9.  Read `/proc/<pid>/status`.
10. Find `VmRSS:` and convert its KiB value into `memory_kb`.
11. Read the process nice value using `getpriority(PRIO_PROCESS, pid)`,
    handling `-1` correctly with `errno`.
12. Copy the parsed name safely into `process->name`.
13. Initialize `cpu_usage_percent` to `0.0`; the resource monitor
    calculates it later.
14. Check every file operation and parsing result.
15. Close every opened file on every path.
16. Return `0` only if the required process record was read
    successfully; otherwise return `-1`.

#### Step A3: Implement `process_manager_get_all`

1.  Create `include/process_manager.h` with the exact declaration.
2.  Create `src/process_manager.c`.
3.  Validate `processes != NULL` and `max_count > 0`.
4.  Open `/proc` using `opendir`.
5.  Iterate through entries using `readdir`.
6.  Identify entries whose names consist entirely of decimal digits.
7.  Convert the directory name to a PID with error checking.
8.  Call `proc_reader_read_process` for each PID.
9.  If a process disappears or access is denied during the read, skip
    that entry and continue.
10. Append only successful records.
11. Never write beyond `max_count`.
12. Close the directory.
13. Return the number of records collected. Return `-1` only if `/proc`
    cannot be enumerated at all.

#### Step A4: Implement `resource_monitor_update`

1.  Create `include/resource_monitor.h` with the exact declaration.

2.  Create `src/resource_monitor.c`.

3.  Validate array pointers, counts and `elapsed_seconds`.

4.  Get clock ticks per second using `sysconf(_SC_CLK_TCK)`.

5.  For each current process, find the previous record with the same
    PID.

6.  If no previous PID exists, set CPU usage to `0.0`.

7.  If current CPU ticks are lower than previous CPU ticks, set usage to
    `0.0`.

8.  If elapsed time or clock tick rate is invalid, set usage to `0.0`.

9.  Otherwise apply the exact formula in `MASTER_SPEC.md`:

    `((current.total_cpu_ticks - previous.total_cpu_ticks) / ticks_per_second) / elapsed_seconds * 100.0`

10. Do not divide by total system CPU ticks or CPU core count. The
    agreed metric is percentage of one logical CPU.

11. Do not change memory values in this function; memory is already read
    by the proc reader.

#### Step A5: Implement controlled test programs

`tests/cpu_hog.c`: 1. Print PID. 2. Run a CPU-intensive loop. 3. Exit
cleanly on SIGTERM. 4. Do not allocate memory continuously.

`tests/memory_growth.c`: 1. Print PID. 2. Allocate a fixed increment (1
MiB) periodically. 3. Touch allocated pages. 4. Stop at 64 MiB. 5.
Handle allocation failure. 6. Free memory on graceful exit.

### Phase B --- Independent testing

Do not wait for other members' modules. Write tests that include only
your headers and use the shared `ProcessInfo` structure.

#### Step B1: Process reader tests

1.  Call `proc_reader_read_process(getpid(), &process)`.
2.  Verify returned PID equals `getpid()`.
3.  Verify name is null-terminated and non-empty.
4.  Verify PPID is non-negative.
5.  Verify state is populated.
6.  Verify memory is non-negative when VmRSS is available.
7.  Test an invalid PID and confirm the function returns `-1`.
8.  Run while a short-lived process exits and confirm failure is handled
    without a crash.

#### Step B2: Process enumeration tests

1.  Call `process_manager_get_all(processes, MAX_PROCESSES)`.
2.  Confirm a non-negative count.
3.  Confirm each returned PID is positive.
4.  Confirm there are no duplicate PIDs.
5.  Call with a small capacity and verify no overflow occurs.
6.  Run enumeration repeatedly while test processes start/exit.

#### Step B3: CPU sampling tests

1.  Create two `ProcessInfo` arrays with the same PID.
2.  Set previous `total_cpu_ticks` to 100 and current to 150.
3.  Use an elapsed time and tick rate that make the expected result
    calculable; do not assume the machine's tick rate.
4.  Compare the result against the formula using a small floating-point
    tolerance.
5.  Test a newly discovered PID.
6.  Test invalid elapsed time.
7.  Test decreasing CPU counters.

#### Step B4: Test programs

1.  Compile `cpu_hog.c` separately.
2.  Run it and record its PID.
3.  Confirm your process reader can read it.
4.  Confirm repeated snapshots show increasing CPU ticks.
5.  Send SIGTERM to this test process only.
6.  Compile and run `memory_growth.c`.
7.  Confirm VmRSS increases across samples until the cap.
8.  Stop the test process gracefully.

#### Step B5: Build checks

Compile your module objects with:

`gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude -c src/proc_reader.c src/process_manager.c src/resource_monitor.c`

Fix all warnings caused by your code. Do not link the final application
yet; `main.c` is Member 3's responsibility.

### Phase C --- Integration

1.  Pull the latest `main` after your pull request is approved/merged
    according to the team's agreed merge order.
2.  Do not edit Member 2 or Member 3 files to make integration compile.
3.  Confirm the shared `ProcessInfo` fields match the master
    specification.
4.  Confirm Member 3 can call `process_manager_get_all`.
5.  Confirm Member 3 can pass current and previous arrays to
    `resource_monitor_update`.
6.  Run the full build when Member 3 has integrated the other modules.
7.  If an API mismatch exists, stop and report the exact mismatch. Do
    not silently alter the API.
8.  Test process enumeration while `cpu_hog`, `memory_growth` and
    `child_tree` are running.
9.  Verify processes that exit between samples are skipped safely.
10. Report test commands and actual results to the team.

## 5. Completion checklist

-   [ ] All three headers and source files exist.
-   [ ] Exact APIs implemented.
-   [ ] `/proc/[pid]/stat` command parsing handles spaces/parentheses.
-   [ ] VmRSS is read from `/proc/[pid]/status`.
-   [ ] Nice value is read with correct errno handling.
-   [ ] Process enumeration handles disappearing processes.
-   [ ] CPU usage uses two samples and the specified formula.
-   [ ] CPU hog and memory growth test programs work.
-   [ ] Module tests pass.
-   [ ] No files outside your ownership were modified.
-   [ ] Changes pushed on `feature/monitoring` with a pull request.
