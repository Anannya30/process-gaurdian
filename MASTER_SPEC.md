# OS Process Guardian --- Master Specification

**Status:** Baseline implementation contract\
**Language:** C (C11)\
**Target:** Linux (development and demonstration through Ubuntu on WSL)\
**Build:** GCC + Make\
**Terminal UI:** ncurses\
**Repository rule:** This document is the source of truth for
architecture, interfaces, naming, ownership and integration.

------------------------------------------------------------------------

## 1. Purpose and operating rules

OS Process Guardian is a Linux user-space program that observes real
processes, collects CPU and memory information, reconstructs process
relationships, tracks process-state changes, detects persistent resource
anomalies, optionally applies a controlled priority adjustment, and
records events.

It does not replace the Linux kernel scheduler, implement a kernel,
perform kernel-memory manipulation, or promise production-grade
system-administration safety.

### 1.1 Non-negotiable rules for every AI coding agent

1.  Implement in **C only**. Do not introduce C++, Python, JavaScript,
    shell-based application logic, or a web application.
2.  Use the Linux `/proc` filesystem and documented POSIX/Linux
    interfaces.
3.  Use ncurses for the interactive terminal dashboard.
4.  Follow the exact file ownership, identifiers, function signatures
    and data structures in this document.
5.  Do not rename, remove, move, or change the type/order of a specified
    structure field, function, parameter, return type, constant, file,
    or directory without explicit human approval.
6.  Do not modify another member's owned implementation files.
7.  Do not add third-party libraries or change the build system without
    approval.
8.  Do not silently change requirements, architecture, thresholds, key
    bindings, safety rules, or APIs.
9.  Do not skip a numbered implementation or test step.
10. Do not claim a test passed unless it was actually executed and its
    output checked.
11. If a requirement is ambiguous, contradictory, impossible, or would
    require changing a shared interface, stop and ask the human. Do not
    guess.
12. Before editing, inspect the current repository and `git status`.
    Never overwrite uncommitted work.
13. Keep changes limited to the current assigned step. Do not perform
    future steps early.
14. Do not automatically terminate processes because of an anomaly.
    Automatic intervention, when enabled, may only lower a process's
    nice priority as specified below. Stop, continue and terminate are
    explicit user commands.
15. Do not run destructive process-control tests against arbitrary
    system processes. Use test processes created by the project team and
    owned by the current user.

### 1.2 Conflict resolution

If a member-specific file appears to disagree with this master file,
stop and ask. The master file is authoritative for shared interfaces. A
human must approve and update both documents before implementation
continues.

------------------------------------------------------------------------

## 2. Locked technical choices

  -----------------------------------------------------------------------
  Concern                             Decision
  ----------------------------------- -----------------------------------
  Language standard                   C11

  Compiler                            GCC

  Platform                            Linux; Ubuntu on WSL is the
                                      development environment

  Build                               Makefile

  Terminal UI                         ncurses

  Process discovery                   Numeric directories under `/proc`

  Process metadata                    `/proc/[pid]/stat` and
                                      `/proc/[pid]/status`

  Process control                     `kill()` with SIGSTOP, SIGCONT and
                                      SIGTERM

  Priority                            `getpriority()` and `setpriority()`

  Sampling                            Single-threaded periodic loop in
                                      the first version

  Logging                             Append-only CSV file at
                                      `logs/process_guardian.csv`

  Test strategy                       Small controlled C test programs
                                      plus module-level tests
  -----------------------------------------------------------------------

No web server, browser interface, database, or external monitoring
service is part of the baseline.

------------------------------------------------------------------------

## 3. Directory and file ownership

The repository structure is fixed as follows:

``` text
process-guardian/
├── include/
│   ├── common.h
│   ├── process_info.h
│   ├── process_manager.h
│   ├── proc_reader.h
│   ├── resource_monitor.h
│   ├── process_tree.h
│   ├── state_tracker.h
│   ├── process_control.h
│   ├── priority_manager.h
│   ├── logger.h
│   ├── anomaly_detector.h
│   ├── controller.h
│   └── dashboard.h
├── src/
│   ├── process_manager.c
│   ├── proc_reader.c
│   ├── resource_monitor.c
│   ├── process_tree.c
│   ├── state_tracker.c
│   ├── process_control.c
│   ├── priority_manager.c
│   ├── logger.c
│   ├── anomaly_detector.c
│   ├── controller.c
│   ├── dashboard.c
│   └── main.c
├── tests/
│   ├── test_process_manager.c
│   ├── test_resource_monitor.c
│   ├── test_process_tree.c
│   ├── test_state_tracker.c
│   ├── test_process_control.c
│   ├── test_priority_manager.c
│   ├── test_logger.c
│   ├── test_anomaly_detector.c
│   ├── cpu_hog.c
│   ├── memory_growth.c
│   ├── child_tree.c
│   └── sleep_test.c
├── logs/
│   └── .gitkeep
├── docs/
│   └── .gitkeep
├── .gitignore
├── Makefile
├── README.md
├── MASTER_SPEC.md
├── MEMBER_1.md
├── MEMBER_2.md
└── MEMBER_3.md
```

### 3.1 Ownership table

  -----------------------------------------------------------------------
  Files                               Owner
  ----------------------------------- -----------------------------------
  `include/common.h`,                 Shared: all members agree before
  `include/process_info.h`            changes

  `include/process_manager.h`,        Member 1
  `include/proc_reader.h`,            
  `include/resource_monitor.h`        

  `src/process_manager.c`,            Member 1
  `src/proc_reader.c`,                
  `src/resource_monitor.c`            

  `tests/test_process_manager.c`,     Member 1
  `tests/test_resource_monitor.c`,    
  `tests/cpu_hog.c`,                  
  `tests/memory_growth.c`             

  `include/process_tree.h`,           Member 2
  `include/state_tracker.h`,          
  `include/process_control.h`,        
  `include/priority_manager.h`,       
  `include/logger.h`                  

  `src/process_tree.c`,               Member 2
  `src/state_tracker.c`,              
  `src/process_control.c`,            
  `src/priority_manager.c`,           
  `src/logger.c`                      

  `tests/test_process_tree.c`,        Member 2
  `tests/test_state_tracker.c`,       
  `tests/test_process_control.c`,     
  `tests/test_priority_manager.c`,    
  `tests/test_logger.c`,              
  `tests/child_tree.c`,               
  `tests/sleep_test.c`                

  `include/anomaly_detector.h`,       Member 3
  `include/controller.h`,             
  `include/dashboard.h`               

  `src/anomaly_detector.c`,           Member 3
  `src/controller.c`,                 
  `src/dashboard.c`, `src/main.c`     

  `tests/test_anomaly_detector.c`     Member 3

  `Makefile`, `.gitignore`,           Shared; changes require agreement
  `README.md`, `MASTER_SPEC.md`,      
  `MEMBER_1.md`, `MEMBER_2.md`,       
  `MEMBER_3.md`                       
  -----------------------------------------------------------------------

**Important:** File ownership does not mean a member can change shared
interfaces independently. All shared headers must remain compatible with
this specification.

------------------------------------------------------------------------

## 4. Shared constants and data structures

### 4.1 `include/common.h`

Define these constants exactly:

``` c
#ifndef COMMON_H
#define COMMON_H

#define MAX_PROCESSES 4096
#define MAX_EVENTS 256
#define MAX_PROCESS_NAME 256
#define MAX_EVENT_TEXT 256
#define DEFAULT_SAMPLE_INTERVAL_MS 1000

#endif
```

Do not create duplicate definitions of these constants in module files.

### 4.2 `include/process_info.h`

This is the shared process record. Every module must use this exact
structure and field names.

``` c
#ifndef PROCESS_INFO_H
#define PROCESS_INFO_H

#include <sys/types.h>
#include "common.h"

typedef struct {
    pid_t pid;
    pid_t ppid;
    char name[MAX_PROCESS_NAME];
    char state;
    int priority;
    unsigned long long user_ticks;
    unsigned long long kernel_ticks;
    unsigned long long total_cpu_ticks;
    long memory_kb;
    double cpu_usage_percent;
} ProcessInfo;

#endif
```

`MAX_PROCESS_NAME` is supplied by `common.h`; include `common.h` before
using the structure or include it directly in `process_info.h`.

Field meanings:

  -----------------------------------------------------------------------
  Field                               Meaning
  ----------------------------------- -----------------------------------
  `pid`                               Process ID

  `ppid`                              Parent process ID

  `name`                              Process command/name,
                                      null-terminated

  `state`                             Linux process state character from
                                      `/proc/[pid]/stat`

  `priority`                          Nice value, normally in the range
                                      -20 to 19

  `user_ticks`                        User CPU time from
                                      `/proc/[pid]/stat`

  `kernel_ticks`                      Kernel CPU time from
                                      `/proc/[pid]/stat`

  `total_cpu_ticks`                   `user_ticks + kernel_ticks`

  `memory_kb`                         Resident memory in KiB

  `cpu_usage_percent`                 CPU usage relative to one logical
                                      CPU; can exceed 100% for
                                      multithreaded processes
  -----------------------------------------------------------------------

Use `-1` for unavailable numeric fields where appropriate and document
the error; do not silently substitute zero for a failed read.

### 4.3 `include/process_tree.h` data structure

Member 2 must define:

``` c
typedef struct {
    pid_t pid;
    pid_t ppid;
    int parent_index;
    int depth;
} ProcessTreeNode;
```

`parent_index` is the index of the parent in the `ProcessTreeNode`
output array, or `-1` if the parent is not in the supplied process list.
`depth` is zero for a root in the displayed subset.

### 4.4 `include/state_tracker.h` data structure

Member 2 must define:

``` c
typedef struct {
    pid_t pid;
    char old_state;
    char new_state;
    int exited;
} ProcessStateChange;
```

For an exited process, set `exited = 1`; `new_state` may be `'\0'`. For
an ordinary state change, set `exited = 0`.

### 4.5 `include/logger.h` data structure

Member 2 must define:

``` c
typedef struct {
    time_t timestamp;
    pid_t pid;
    char event_type[32];
    char message[MAX_EVENT_TEXT];
    double measured_value;
    double threshold;
    char action[64];
    char result[128];
} GuardianEvent;
```

Include `<time.h>`, `<sys/types.h>` and `common.h`.

------------------------------------------------------------------------

## 5. Exact module interfaces

The declarations below are the API contract. Implementations must match
them exactly.

### 5.1 Member 1 interfaces

`include/proc_reader.h`

``` c
#ifndef PROC_READER_H
#define PROC_READER_H

#include "process_info.h"

int proc_reader_read_process(pid_t pid, ProcessInfo *process);

#endif
```

`proc_reader_read_process` returns `0` on success and `-1` on failure.
It must safely parse `/proc/[pid]/stat`, including command names
containing spaces or parentheses, and read resident memory from
`/proc/[pid]/status` (VmRSS). It must check file and parsing errors.

`include/process_manager.h`

``` c
#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include "process_info.h"

int process_manager_get_all(ProcessInfo processes[], int max_count);

#endif
```

`process_manager_get_all` scans numeric `/proc` directories, calls the
proc reader, skips processes that disappear or cannot be read, and
returns the number successfully collected. Return `-1` only for a
top-level failure that prevents enumeration. Never write beyond
`max_count`.

`include/resource_monitor.h`

``` c
#ifndef RESOURCE_MONITOR_H
#define RESOURCE_MONITOR_H

#include "process_info.h"

void resource_monitor_update(
    ProcessInfo current[],
    int current_count,
    const ProcessInfo previous[],
    int previous_count,
    double elapsed_seconds
);

#endif
```

For each current PID also present in the previous snapshot, calculate:

`cpu_usage_percent = ((current.total_cpu_ticks - previous.total_cpu_ticks) / ticks_per_second) / elapsed_seconds * 100.0`

Use `sysconf(_SC_CLK_TCK)` for `ticks_per_second`. If a process is new,
counters decreased, elapsed time is invalid, or a calculation cannot be
made, set usage to `0.0`. Keep this definition consistent; do not
normalize by total system CPU time.

### 5.2 Member 2 interfaces

`include/process_tree.h`

``` c
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
```

Return number of nodes created, or `-1` for invalid arguments. Preserve
each process once. A parent not present in the supplied list has
`parent_index = -1`. Avoid infinite recursion/loops if malformed parent
relationships are encountered.

`include/state_tracker.h`

``` c
#ifndef STATE_TRACKER_H
#define STATE_TRACKER_H

#include "process_info.h"

typedef struct {
    pid_t pid;
    char old_state;
    char new_state;
    int exited;
} ProcessStateChange;

int state_tracker_find_changes(
    const ProcessInfo current[],
    int current_count,
    const ProcessInfo previous[],
    int previous_count,
    ProcessStateChange changes[],
    int max_changes
);

#endif
```

Return the number of changes, or `-1` for invalid arguments. Report
state changes for PIDs present in both snapshots and exited processes
that were present previously but are absent now. Newly discovered
processes are not state changes.

`include/process_control.h`

``` c
#ifndef PROCESS_CONTROL_H
#define PROCESS_CONTROL_H

#include <sys/types.h>

int process_control_stop(pid_t pid);
int process_control_resume(pid_t pid);
int process_control_terminate(pid_t pid);

#endif
```

Each function returns `0` on successful signal delivery and `-1` on
failure. Use SIGSTOP, SIGCONT and SIGTERM respectively. Do not silently
retry with SIGKILL. Do not claim that signal delivery proves the process
has completed the requested state transition.

`include/priority_manager.h`

``` c
#ifndef PRIORITY_MANAGER_H
#define PRIORITY_MANAGER_H

#include <sys/types.h>

int priority_manager_get(pid_t pid, int *nice_value);
int priority_manager_set(pid_t pid, int nice_value);

#endif
```

Return `0` on success and `-1` on failure. Set only values in \[-20,
19\]; reject out-of-range values. Correctly handle `getpriority()`'s
`-1` return by checking `errno`.

`include/logger.h`

``` c
#ifndef LOGGER_H
#define LOGGER_H

#include "common.h"
#include <time.h>
#include <sys/types.h>

typedef struct {
    time_t timestamp;
    pid_t pid;
    char event_type[32];
    char message[MAX_EVENT_TEXT];
    double measured_value;
    double threshold;
    char action[64];
    char result[128];
} GuardianEvent;

int logger_init(const char *file_path);
int logger_log_event(const GuardianEvent *event);
void logger_close(void);

#endif
```

Write CSV records to the path passed to `logger_init`, opening in append
mode. Write a header only when the file is empty/new. Quote and escape
CSV text fields correctly. Return `0` on success and `-1` on failure.
`logger_close` is safe to call after successful initialization; it must
not crash if called when not initialized.

### 5.3 Member 3 interfaces

`include/anomaly_detector.h`

``` c
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
```

Rules: - CPU anomaly: `cpu_usage_percent` must be strictly greater than
`cpu_threshold_percent` for `required_consecutive_samples` consecutive
updates. - Memory-growth anomaly: memory must increase by at least
`memory_growth_threshold_kb` compared with the previous sample for
`required_consecutive_samples` consecutive updates. - Reset the relevant
counter when its condition is not met. - Do not emit a new event for the
same PID and anomaly type on every consecutive update while the
condition remains continuously active. Emit once when the condition
first becomes confirmed; allow a new event after the condition clears
and later becomes confirmed again. - Remove or recycle tracker entries
for PIDs no longer present so the fixed tracker array cannot grow
indefinitely. - Return event count, or `-1` for invalid arguments. -
Validate configuration values. Do not invent defaults inside the update
function.

`include/controller.h`

``` c
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
```

Rules: - If `auto_lower_priority == 0`, report-only: do not change
process priority; write a clear action/result string. - If
`auto_lower_priority == 1`, read current nice value and request a lower
scheduling priority by adding `nice_increment`, capped at 19. Do not use
a negative increment. Call Member 2's `priority_manager_get` and
`priority_manager_set`. - Never stop, resume or terminate a process
automatically. - Return `0` when the controller handled the event
(including report-only); return `-1` for invalid input or an operation
failure. Set `new_nice_value` and `action_result` consistently. - Do not
log directly here. The integration layer (`main.c`) creates and logs the
final `GuardianEvent`.

`include/dashboard.h`

``` c
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
```

Dashboard requirements: - Use ncurses; initialize and restore terminal
state correctly. - Display a process table with PID, PPID, name, state,
CPU%, memory and priority. - Display the process tree view, recent
anomaly alerts and basic status/help text. - Allow row selection with
up/down arrow keys. - Keys: `q` quit; `t` toggle process/tree view; `s`
stop selected PID; `c` continue selected PID; `k` request termination of
selected PID after confirmation; `p` request a nice value change; `l`
toggle a recent event/log view if available. - Dashboard must not
directly implement process-control or priority system calls. It reports
user input to `main.c`; `main.c` invokes Member 2 APIs. - Do not make
terminal UI calls from monitoring or detector modules. - Handle terminal
resize and empty process lists without crashing. - Never display
uninitialized array entries.

**Input contract:** `dashboard_get_input()` returns the ncurses key code
(or `ERR` when no key is available). `dashboard_get_selected_pid()`
returns selected PID or `-1` when no row is selected. For `p`, `main.c`
prompts for the requested nice value using ncurses and validates it
before calling the priority API.

------------------------------------------------------------------------

## 6. Main program integration contract

Only Member 3 owns `src/main.c`. `main.c` is the composition root: it
calls modules but must not duplicate their internal algorithms.

### 6.1 Required startup sequence

1.  Set default configuration values:
    -   sample interval: `DEFAULT_SAMPLE_INTERVAL_MS`
    -   CPU threshold: `80.0`
    -   memory growth threshold: `10240` KiB per sample
    -   required consecutive samples: `3`
    -   automatic priority lowering: disabled (`0`)
    -   nice increment: `5`
2.  Initialize logger at `logs/process_guardian.csv`.
3.  Initialize ncurses dashboard.
4.  Collect initial process snapshot.
5.  Begin periodic monitoring.

These are initial project defaults, not claims that the thresholds are
universally correct. They may only be changed after human approval.

### 6.2 Monitoring loop

Each iteration must:

1.  Record the start time using a monotonic clock.
2.  Discover current processes with `process_manager_get_all`.
3.  Calculate elapsed time since the prior successful sample.
4.  Call `resource_monitor_update` with current and previous snapshots.
5.  Call `process_tree_build`.
6.  Call `state_tracker_find_changes`.
7.  Convert state changes to `GuardianEvent` records and log them.
8.  Call `anomaly_detector_update`.
9.  For each anomaly, call `controller_handle_anomaly`, create a
    `GuardianEvent` containing measured value, threshold, chosen action
    and result, and log it.
10. Render the dashboard.
11. Read keyboard input without blocking the sampling loop.
12. Handle user commands using Member 2 APIs. Log each requested action
    and its result.
13. Wait for the configured interval without busy-spinning.
14. Copy the current process snapshot to the previous snapshot for the
    next iteration.

Use a monotonic time source for elapsed-time calculations. Handle
process disappearance between discovery and control calls. If process
enumeration fails, show/log the error and continue safely where
possible; do not use stale uninitialized data.

### 6.3 Memory and array handling

-   Use fixed arrays sized by `MAX_PROCESSES` and `MAX_EVENTS` for the
    baseline.
-   Check every returned count before indexing.
-   Do not write beyond array capacity.
-   If a count reaches capacity, report truncation safely.
-   Do not retain pointers into temporary stack buffers.
-   Keep the previous process snapshot separate from the current
    snapshot.

------------------------------------------------------------------------

## 7. Test program specifications

All test programs are controlled inputs; they are not part of the
Guardian executable.

### 7.1 `tests/cpu_hog.c` --- Member 1

-   Print its PID at startup.
-   Run a CPU-intensive loop until interrupted.
-   Support graceful exit on SIGTERM.
-   Do not spawn children or allocate unbounded memory.

### 7.2 `tests/memory_growth.c` --- Member 1

-   Print its PID.
-   Allocate memory in fixed increments (for example 1 MiB per second).
-   Touch each allocated page so resident memory increases.
-   Stop at a documented safe cap (for example 64 MiB) and wait for
    SIGTERM.
-   Handle allocation failure and free allocated memory on exit.

### 7.3 `tests/child_tree.c` --- Member 2

-   Parent prints its PID.
-   Create a small fixed number of children using `fork()` (three
    children).
-   Children print PID and PPID and remain alive using `pause()` or a
    controlled sleep loop.
-   Parent reaps children with `wait()`/`waitpid()` and exits cleanly
    after children finish or receive termination.

### 7.4 `tests/sleep_test.c` --- Member 2

-   Print PID and PPID.
-   Sleep in a loop until SIGTERM.
-   Used for stop/resume/terminate tests.

------------------------------------------------------------------------

## 8. Required module test coverage

Tests must be repeatable and must not depend on arbitrary system
processes.

  -----------------------------------------------------------------------
  Module                              Minimum checks
  ----------------------------------- -----------------------------------
  Proc reader/process manager         Read current PID; enumerate
                                      processes; skip
                                      malformed/disappearing entries;
                                      respect max count

  Resource monitor                    New PID gives 0%; known tick delta
                                      gives expected percentage; invalid
                                      elapsed time gives 0%; decreasing
                                      counter handled safely

  Process tree                        Correct parent-child link; missing
                                      parent; root; no duplicate nodes;
                                      malformed relationship does not
                                      loop forever

  State tracker                       State change; exited process; newly
                                      discovered process ignored;
                                      unchanged state ignored

  Process control                     Stop, continue, SIGTERM on owned
                                      `sleep_test`;
                                      permission/process-gone errors
                                      handled

  Priority manager                    Read nice value; valid change on
                                      owned test process; reject values
                                      outside \[-20,19\]; permission
                                      errors handled

  Logger                              New file header; append row;
                                      commas/quotes/newlines escaped;
                                      open/write failure

  Anomaly detector                    CPU below threshold; one high
                                      sample does not trigger; persistent
                                      high samples trigger once; reset
                                      then trigger again; memory growth
                                      persistence; PID cleanup

  Controller                          Report-only does not call priority
                                      setter; enabled policy lowers nice
                                      value; cap at 19; invalid config
                                      rejected; no signal/termination
                                      action
  -----------------------------------------------------------------------

Each member's file specifies their exact test order and local test
strategy.

------------------------------------------------------------------------

## 9. Build and coding conventions

-   Use C11 and compile with warnings enabled:
    `-std=c11 -Wall -Wextra -Wpedantic`.
-   Use `-lncurses` when linking the Guardian executable.
-   Use include guards in every header.
-   Include only required headers; do not rely on accidental transitive
    includes.
-   Check return values from `fopen`, `fgets`, `fscanf`/parsing
    routines, `opendir`, `readdir`, `kill`, `getpriority`,
    `setpriority`, `clock_gettime`, and memory allocation.
-   Use `snprintf` rather than unsafe unbounded string formatting.
-   Do not use global mutable state unless the interface explicitly
    requires it.
-   Keep functions focused and beginner-readable. Prefer loops and clear
    conditionals over compressed expressions.
-   Add comments for non-obvious parsing, calculations and OS behavior,
    not comments that merely restate each line.
-   Do not suppress compiler warnings to make a build appear clean.
-   All public function declarations belong in their assigned headers.

------------------------------------------------------------------------

## 10. Git and collaboration contract

Branches:

-   Member 1: `feature/monitoring`
-   Member 2: `feature/process-control`
-   Member 3: `feature/detection-ui`

Rules:

1.  `main` is protected by team agreement; members work on their own
    feature branch.
2.  Pull the latest `main` before starting a task.
3.  Commit only files owned by the member, except approved shared
    documentation/header changes.
4.  Use small commits with meaningful messages.
5.  Push the feature branch and open a pull request.
6.  Review the diff before merging.
7.  Do not merge a module before its standalone tests pass.
8.  Do not resolve a conflict by blindly choosing "ours" or "theirs";
    inspect and ask if ownership or intended behavior is unclear.
9.  Member 3 coordinates the final integration because `main.c` composes
    the modules. This does not make Member 3 responsible for rewriting
    other members' modules.
10. All three members participate in integration testing and bug fixing
    within their own modules.

------------------------------------------------------------------------

## 11. Definition of done

The baseline is complete only when:

-   [ ] The project builds using the agreed Makefile and GCC.
-   [ ] Guardian launches in the terminal using ncurses.
-   [ ] Process table shows PID, PPID, name, state, CPU%, memory and
    priority.
-   [ ] Process tree can be displayed.
-   [ ] CPU usage is based on consecutive samples, not a single
    cumulative counter.
-   [ ] Memory is sampled repeatedly.
-   [ ] State transitions and process exits are detected.
-   [ ] User can stop, continue and request SIGTERM for an owned test
    process.
-   [ ] User can inspect/change nice priority subject to OS permissions.
-   [ ] Persistent CPU and memory-growth anomalies are detected only
    after the configured consecutive samples.
-   [ ] Automatic priority lowering is disabled by default and never
    terminates a process.
-   [ ] Events are written to the CSV log.
-   [ ] Process disappearance, permission errors and invalid PIDs do not
    crash the application.
-   [ ] All module tests pass.
-   [ ] The team completes the documented final demonstration using
    controlled test programs.

------------------------------------------------------------------------

## 12. Final demonstration sequence

1.  Build and launch Process Guardian.
2.  Show the live process table.
3.  Launch `child_tree` and show its hierarchy.
4.  Launch `cpu_hog` and identify its PID.
5.  Show CPU sampling across multiple intervals.
6.  Confirm the CPU anomaly appears only after the persistence
    requirement is met.
7.  Show the report-only action by default.
8.  If the team enables automatic priority lowering for the
    demonstration, show the nice-value change on the test process only.
9.  Show the event log.
10. Stop and resume `sleep_test` from the dashboard and show the state
    change.
11. Request graceful termination of the test process and show the
    result.

Do not use an arbitrary system process for the demonstration.

------------------------------------------------------------------------

## 13. Change-control procedure

If an agent believes a change is necessary:

1.  Stop before making the change.
2.  State the exact file, identifier, interface or requirement that
    appears problematic.
3.  Explain why the current specification cannot be followed.
4.  Propose the smallest possible change and identify all affected
    members/files.
5.  Wait for explicit human approval.
6.  After approval, update `MASTER_SPEC.md` and affected member files
    before changing implementation code.

No agent may treat a suggestion, assumption, compiler warning, or
convenience as permission to alter the specification.
