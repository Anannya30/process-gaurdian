# OS Process Guardian — Comprehensive Technical Specification, Architecture & Project Report

**Project Title:** OS Process Guardian  
**Target Platform:** Linux (Ubuntu on WSL / Native Linux)  
**Language Standard:** C (C11, POSIX.1-2008)  
**Compiler & Build System:** GCC (`-std=c11 -Wall -Wextra -Wpedantic`) + GNU Make  
**UI Framework:** ncurses (Terminal-based Dashboard)  
**Primary Observation Interface:** Linux `/proc` Virtual Filesystem  
**Event Logging:** Append-only CSV Audit Log (`logs/process_guardian.csv`)  
**Repository Branch:** `main` (clean, fully synchronized)

---

## 1. Executive Summary & Purpose

**OS Process Guardian** is a robust, modular, user-space system monitoring and control suite designed for Linux. It observes live system processes, extracts CPU tick counters and resident memory statistics, reconstructs hierarchical parent-child process relationships, tracks runtime process state changes, detects persistent resource anomalies, optionally enforces controlled priority adjustments (via Linux nice values), and provides a keyboard-driven interactive terminal dashboard with real-time alerts and comprehensive audit logging.

### Key Architectural Tenets
1. **Strict Separation of Concerns:** Divided across three specialized team modules (Member 1, Member 2, and Member 3), strictly adhering to a rigid ownership matrix.
2. **Deterministic Resource Bounds:** Pre-allocated, fixed-size structures (`MAX_PROCESSES = 4096`, `MAX_EVENTS = 256`) to ensure zero unbounded heap allocations in the critical monitoring loop.
3. **True Anomaly Persistence & Suppression:** Anomalies require three consecutive sampling cycles of threshold violations before triggering. Once confirmed, repeated emissions are suppressed while the violation remains continuous, avoiding notification floods.
4. **POSIX Non-Destructive Safety Rules:** Process termination (`SIGTERM`) is never triggered autonomously; it requires explicit interactive human confirmation. Automatic mitigation is strictly confined to priority de-escalation (increasing the process nice value up to a cap of 19).

---

## 2. Team File Ownership & Interface Matrix

| Component / Module | Files Owned | Primary API Functions | Responsible Member |
| :--- | :--- | :--- | :--- |
| **Process Discovery & Metric Sampling** | `include/proc_reader.h`<br>`include/process_manager.h`<br>`include/resource_monitor.h`<br>`src/proc_reader.c`<br>`src/process_manager.c`<br>`src/resource_monitor.c`<br>`tests/cpu_hog.c`<br>`tests/memory_growth.c`<br>`tests/test_process_manager.c`<br>`tests/test_resource_monitor.c` | `proc_reader_read_process()`<br>`process_manager_get_all()`<br>`resource_monitor_update()` | **Member 1**<br>*(Anannya Devale)* |
| **Hierarchy, State Tracking & Control** | `include/process_tree.h`<br>`include/state_tracker.h`<br>`include/process_control.h`<br>`include/priority_manager.h`<br>`include/logger.h`<br>`src/process_tree.c`<br>`src/state_tracker.c`<br>`src/process_control.c`<br>`src/priority_manager.c`<br>`src/logger.c`<br>`tests/child_tree.c`<br>`tests/sleep_test.c`<br>`tests/test_*.c` | `process_tree_build()`<br>`state_tracker_find_changes()`<br>`process_control_stop()`<br>`process_control_resume()`<br>`process_control_terminate()`<br>`priority_manager_get()`<br>`priority_manager_set()`<br>`logger_init()`, `logger_log_event()`, `logger_close()` | **Member 2**<br>*(Tanvi Modi)* |
| **Anomaly Engine, Dashboard & Integration** | `include/anomaly_detector.h`<br>`include/controller.h`<br>`include/dashboard.h`<br>`src/anomaly_detector.c`<br>`src/controller.c`<br>`src/dashboard.c`<br>`src/main.c`<br>`tests/test_anomaly_detector.c`<br>`tests/test_controller.c`<br>`tests/test_dashboard_standalone.c` | `anomaly_detector_init()`<br>`anomaly_detector_update()`<br>`controller_handle_anomaly()`<br>`dashboard_init()`, `dashboard_render()`, `dashboard_get_input()`, `dashboard_shutdown()`<br>`main()` (Monotonic Event Loop) | **Member 3**<br>*(Yukta Mahajan)* |
| **Shared Data Contracts** | `include/common.h`<br>`include/process_info.h` | *Locked structures and constants across all members* | **Shared Agreement** |

---

## 3. Data Structures & Architectural Data Flow

### 3.1 Primary Data Structures

#### `ProcessInfo` (`include/process_info.h`)
The universal process record captured on each sampling iteration:
```c
typedef struct {
    pid_t pid;                           // Process identifier
    pid_t ppid;                          // Parent process identifier
    char name[MAX_PROCESS_NAME];         // Process executable command (from stat comm)
    char state;                          // Single-character process state: R, S, D, Z, T
    int priority;                        // Process nice priority (-20 to 19)
    unsigned long long user_ticks;       // User mode CPU jiffies
    unsigned long long kernel_ticks;     // Kernel/system mode CPU jiffies
    unsigned long long total_cpu_ticks;  // user_ticks + kernel_ticks
    long memory_kb;                      // Resident Set Size (VmRSS in KiB)
    double cpu_usage_percent;            // CPU utilization (% of one logical core)
} ProcessInfo;
```

#### `ProcessTreeNode` (`include/process_tree.h`)
Represents hierarchical tree ancestry:
```c
typedef struct {
    pid_t pid;          // Process PID
    pid_t ppid;         // Parent PID
    int parent_index;   // Index in nodes array (-1 if orphan or parent not in snapshot)
    int depth;          // Tree depth from root (root = 0)
} ProcessTreeNode;
```

#### `ProcessStateChange` (`include/state_tracker.h`)
Captures process state lifecycle transitions:
```c
typedef struct {
    pid_t pid;          // Process PID
    char old_state;     // Previous state character
    char new_state;     // Current state character ('\0' if exited)
    int exited;         // 1 if process was present in previous but absent in current
} ProcessStateChange;
```

#### `AnomalyEvent` (`include/anomaly_detector.h`)
Emitted when persistent violations occur:
```c
typedef struct {
    pid_t pid;                     // Offending process PID
    char type[32];                 // "CPU_HIGH" or "MEMORY_GROWTH"
    double measured_value;         // Current CPU % or current memory in KiB
    double threshold;              // Violated threshold
    char message[MAX_EVENT_TEXT];  // Human-readable diagnostic message
} AnomalyEvent;
```

#### `GuardianEvent` (`include/logger.h`)
Audit log entry structure written to CSV:
```c
typedef struct {
    time_t timestamp;              // Epoch timestamp
    pid_t pid;                     // Process PID
    char event_type[32];           // "CPU_HIGH", "STATE_CHANGE", "USER_STOP", etc.
    char message[MAX_EVENT_TEXT];  // Diagnostic/event description
    double measured_value;         // Measured metric
    double threshold;              // Configured metric limit
    char action[64];               // Action taken ("SIGSTOP", "report_only", etc.)
    char result[128];              // Outcome ("signal_sent", "Nice changed", etc.)
} GuardianEvent;
```

---

## 4. Subsystem Deep Dive & Implementation Details

```
+-----------------------------------------------------------------------------------+
|                            OS PROCESS GUARDIAN RUNTIME LOOP                       |
+-----------------------------------------------------------------------------------+
                                       |
    [1] Periodic Clock (CLOCK_MONOTONIC, DEFAULT_SAMPLE_INTERVAL_MS = 1000ms)
                                       v
    [2] Process Discovery (process_manager_get_all -> opendir/readdir("/proc"))
        |--> Reads /proc/[pid]/stat  (PID, PPID, State, Comm with parentheses, Ticks)
        |--> Reads /proc/[pid]/status (VmRSS in KiB)
        +--> getpriority(PRIO_PROCESS, pid) with errno checking
                                       v
    [3] Resource Monitoring (resource_monitor_update)
        +--> Delta CPU = current.total_ticks - previous.total_ticks
        +--> CPU% = ((Delta / sysconf(_SC_CLK_TCK)) / elapsed_seconds) * 100.0
                                       v
    [4] Hierarchy & State Tracking
        |--> process_tree_build: maps ppid -> parent_index, resolves depth, detects cycles
        +--> state_tracker_find_changes: detects 'R'->'S' changes or process exits (exited=1)
                                       v
    [5] Anomaly Detection Engine (anomaly_detector_update)
        |--> CPU% > 80.0% for 3 consecutive samples => Emits CPU_HIGH
        |--> Memory Delta >= 10,240 KiB for 3 consecutive samples => Emits MEMORY_GROWTH
        +--> ACTIVE SUPPRESSION: Flags once, suppresses duplicates while continuous
                                       v
    [6] Adaptive Controller & Audit Logger
        |--> controller_handle_anomaly: auto_lower_priority or report_only
        +--> logger_log_event: Appends RFC-4180 CSV row to logs/process_guardian.csv
                                       v
    [7] Interactive ncurses Dashboard (dashboard_render & handle_key)
        |--> Displays Process Table (PID, PPID, Name, State, CPU%, MemKB, Prio)
        |--> Tree View Toggle ('t'), Log View Toggle ('l')
        |--> Keyboard Control: Stop ('s'), Resume ('c'), Terminate with confirm ('k')
        +--> Nice Value Adjustment ('p')
```

### 4.1 Member 1: Discovery & Resource Monitoring

#### 1. Robust `/proc/[pid]/stat` Parsing
Linux command names in `/proc/[pid]/stat` can contain spaces and parentheses (e.g., `(Web Content)` or `(cat)`). Naive `sscanf` or whitespace tokenization fails catastrophically.
- **Solution:** [`src/proc_reader.c`](file:///c:/Users/Anannya/process-gaurdian/src/proc_reader.c) locates the first `(` with `strchr()` and the last `)` with `strrchr()`. The substring between is extracted safely into `process->name`, and parsing resumes after the final `)`.

#### 2. VmRSS Memory Extraction
Memory is read from `/proc/[pid]/status` by scanning line-by-line for `VmRSS:` and reading the KiB integer. If the process does not have resident memory (e.g., kernel threads), it gracefully records `-1` or `0`.

#### 3. Single-Core Normalized CPU Calculation
Formula implemented in [`src/resource_monitor.c`](file:///c:/Users/Anannya/process-gaurdian/src/resource_monitor.c):
$$\text{CPU \%} = \frac{\frac{\text{current.total\_cpu\_ticks} - \text{previous.total\_cpu\_ticks}}{\text{sysconf}(\_SC\_CLK\_TCK)}}{\text{elapsed\_seconds}} \times 100.0$$
- Validates that `current.total_cpu_ticks >= previous.total_cpu_ticks`.
- Gracefully handles newly discovered PIDs (assigns `0.0%`).
- Does not divide by total host core count; values reflect percentage of one logical CPU core (consistent with `top` default mode).

---

### 4.2 Member 2: Process Tree, State Tracking, Process Control & Logging

#### 1. Hierarchical Tree Construction
- Maps every process's `ppid` to the array index of the matching parent `pid`.
- Computes depth recursively with cycle protection (`visited > process_count`).
- Does not mutate the input array order.

#### 2. Process State Tracking
- Compares state characters across consecutive iterations.
- Detects transitions such as Running (`R`), Sleeping (`S`), Disk Sleep (`D`), Stopped (`T`), Zombie (`Z`).
- Identifies terminating processes present in `previous` but absent in `current`, recording an exit event (`exited = 1`, `new_state = '\0'`).

#### 3. Signal-Based Process Control
- `process_control_stop(pid)`: Sends `kill(pid, SIGSTOP)`.
- `process_control_resume(pid)`: Sends `kill(pid, SIGCONT)`.
- `process_control_terminate(pid)`: Sends `kill(pid, SIGTERM)`.
- Preserves native system `errno` without arbitrary fallback signals like `SIGKILL`.

#### 4. Priority Manager
- `priority_manager_get(pid, &nice)`: Reads nice priority via `getpriority(PRIO_PROCESS, pid)` with `errno = 0` validation to differentiate valid priority `-1` from errors.
- `priority_manager_set(pid, nice)`: Validates range $[-20, 19]$ and applies via `setpriority(PRIO_PROCESS, pid, nice)`.

#### 5. Thread-Safe CSV Event Logger
- Writes append-only logs to `logs/process_guardian.csv`.
- Writes a standard CSV header row on creation.
- Implements RFC 4180 escaping (doubling quotes and quoting strings containing commas, quotes, or newlines).

---

### 4.3 Member 3: Anomaly Engine, Controller, Dashboard & Integration

#### 1. Anomaly Detection Engine
Configured with default parameters:
- `cpu_threshold_percent = 80.0%`
- `memory_growth_threshold_kb = 10,240 KiB` (10 MiB)
- `required_consecutive_samples = 3`

**Active Suppression Mechanism:**
- Increments `cpu_high_count` or `memory_growth_count` each cycle a violation occurs.
- On cycle 3, emits an `AnomalyEvent` and sets `cpu_anomaly_active = 1`.
- On cycles 4, 5, ..., if the process remains high, no duplicate event is emitted.
- If the CPU drops below threshold, `cpu_high_count` resets to 0 and `cpu_anomaly_active` resets to 0, allowing future re-triggering upon recurrence.
- Cleans up stale trackers for exited PIDs by compacting the array in-place.

#### 2. Adaptive Controller
- In report-only mode (`auto_lower_priority == 0`), records the incident without modifying process priority.
- In active mode (`auto_lower_priority == 1`), calls Member 2's priority manager to adjust nice values by `+nice_increment` (capped at 19).

#### 3. ncurses Interactive Dashboard
- Non-blocking keyboard input (`nodelay(stdscr, TRUE)`).
- Scrollable process table with arrow key navigation.
- Color highlights: Cyan title bar, Green selected row, Yellow sections, Red alert banner.
- Key bindings:
  - `q`: Clean shutdown.
  - `t`: Toggle tree view hierarchy.
  - `l`: Toggle audit log view.
  - `s`: Stop selected process (`SIGSTOP`).
  - `c`: Resume selected process (`SIGCONT`).
  - `k`: Terminate selected process (`SIGTERM` with explicit interactive confirmation prompt).
  - `p`: Prompt and set new nice priority.

---

## 5. Test Suite & Validation Evidence

### 5.1 Unit & Integration Test Summary Table

| Test Suite File | Module Under Test | Cases Verified | Result |
| :--- | :--- | :--- | :--- |
| `tests/test_anomaly_detector.c` | Anomaly Detector Engine | 43 test assertions: CPU threshold limits, 3-sample persistence, active duplicate suppression, condition clear & recovery re-triggering, memory baseline, memory growth thresholds, new PID discovery, exited PID tracker cleanup, NULL pointer safety, parameter bounds. | **PASS (43/43)** |
| `tests/test_resource_monitor.c` | CPU Sampling Calculator | Delta tick computation, clock ticks per second conversion, multi-PID snapshots, newly discovered PID zero-initialization, non-monotonic tick protection, invalid elapsed time handling. | **PASS** |
| `tests/test_process_manager.c` | Proc Reader & Enumerator | Live process reading against `getpid()`, name validation, PPID non-negative, state populated, VmRSS parsing, negative PID rejection, non-existent PID rejection, short-lived exited process safety, duplicate PID check, buffer capacity limits. | **PASS** |
| `tests/test_process_tree.c` | Process Tree Builder | Multi-level hierarchy resolution, orphan parent handling (`parent_index = -1`), synthetic cyclic graph detection, input validation. | **PASS** |
| `tests/test_state_tracker.c` | Process State Tracker | Running-to-sleeping state transition detection, exited process detection (`exited = 1`), identical state filtering, boundary checks. | **PASS** |
| `tests/test_process_control.c` | Process Control Signals | Subprocess spawning via `sleep_test`, verified `SIGSTOP`, verified `SIGCONT`, verified `SIGTERM` cleanup and reap via `waitpid()`. | **PASS** |
| `tests/test_priority_manager.c` | Priority Manager | Retrieval of current nice value, priority adjustment within [-20, 19], restoration of original priority. | **PASS** |
| `tests/test_logger.c` | CSV Event Logger | File initialization, header writing, RFC 4180 field quoting, numeric accuracy, file closure. | **PASS** |

### 5.2 Live End-to-End Workload Demonstration

The end-to-end integration was validated using controlled workload generators:

1. **Workload Generator: `tests/cpu_hog.c`**
   - Spawns a dedicated process executing an unthrottled computation loop.
   - Binds a POSIX signal handler to cleanly exit upon `SIGTERM`.
   - Generates steady ~100% CPU utilization on a single core.

2. **Observed Live Execution Behavior:**
   - **Sample 0 (0.0s):** `cpu_hog` discovered in `/proc`, initial ticks recorded. CPU reported as `0.00%`.
   - **Sample 1 (1.0s):** Delta ticks measured; CPU jumps to `99.8%`. Violation counter = 1.
   - **Sample 2 (2.0s):** CPU remains `100.0%`. Violation counter = 2.
   - **Sample 3 (3.0s):** Violation counter reaches 3. Anomaly engine emits `CPU_HIGH`.
     - Dashboard renders bright **RED alert banner**:
       `ALERTS: [CPU_HIGH] PID <pid> CPU 100.00% > 80.00% for 3 consecutive samples`
     - Controller logs incident to `logs/process_guardian.csv`.
   - **Sample 4 (4.0s):** Active suppression suppresses duplicate alert; dashboard returns to monitoring state.
   - **User Intervention:**
     - Operator presses **`s`**: `SIGSTOP` delivered. Process state immediately flips from `R` to `T`. CPU drops to `0.00%`.
     - Condition clears: Anomaly tracker resets counter to 0.
     - Operator presses **`c`**: `SIGCONT` delivered. Process resumes in state `R`.
     - After 3 seconds of resumed CPU load, the anomaly correctly fires again.
     - Operator presses **`k`**, followed by **`y`**: Process terminates gracefully, is reaped, and disappears from the dashboard.

---

## 6. How to Build, Run & Demonstrate (Reproducibility Guide)

### 6.1 Prerequisites
- Linux or Windows with WSL2 (Ubuntu 20.04 / 22.04 / 24.04)
- Packages: `gcc`, `make`, `libncurses5-dev`, `libncursesw5-dev`

### 6.2 Compilation & Execution Commands
```bash
# 1. Clone & navigate to the repository
cd process-gaurdian

# 2. Build the main application binary
make clean
make

# 3. Build test suite binaries
gcc -std=c11 -Wall -Wextra -Iinclude src/proc_reader.c src/process_manager.c tests/test_process_manager.c -o test_pm
gcc -std=c11 -Wall -Wextra -Iinclude src/resource_monitor.c tests/test_resource_monitor.c -o test_rm -lm
gcc -std=c11 -Wall -Wextra -Iinclude src/anomaly_detector.c tests/test_anomaly_detector.c -o test_anomaly
gcc -std=c11 -Wall -Wextra tests/cpu_hog.c -o tests/cpu_hog
gcc -std=c11 -Wall -Wextra tests/memory_growth.c -o tests/memory_growth

# 4. Execute Unit Tests
./test_pm
./test_rm
./test_anomaly

# 5. Run Live End-to-End System Demo
./tests/cpu_hog &              # Start background CPU load (prints PID)
./process_guardian             # Launch live ncurses Guardian UI
```

---

## 7. Slide Presentation Blueprint (Context for Next Agent)

When preparing presentation slides, use the following 10-slide outline:

- **Slide 1: Title & Authors**
  - Project Title: *OS Process Guardian: User-Space Linux Process & Anomaly Management System*
  - Team: Anannya Devale (Member 1), Tanvi Modi (Member 2), Yukta Mahajan (Member 3)
- **Slide 2: Problem Statement & Objectives**
  - Challenges in modern Linux system monitoring: high overhead of invasive agents, cryptic `/proc` data structures, noisy false-positive alert storms.
  - Project Goal: High-efficiency, zero-dependency C11 user-space guardian with strict persistence validation.
- **Slide 3: System Architecture & Workflow**
  - Pipeline diagram: Discovery -> Metrics -> Ancestry -> State Changes -> Anomaly Detection -> Adaptive Control -> Logging -> UI.
- **Slide 4: Module 1 — Process Discovery & Resource Metrics (Member 1)**
  - Safe extraction from `/proc/[pid]/stat` (solving parentheses in process names).
  - Accurate single-core normalized CPU utilization & resident memory (VmRSS) extraction.
- **Slide 5: Module 2 — Process Hierarchy & State Tracking (Member 2)**
  - Tree ancestry graph reconstruction and depth determination.
  - State machine observing transitions (`R`, `S`, `D`, `Z`, `T`) and exit events.
- **Slide 6: Module 2 — Process Control, Priority & Logging (Member 2)**
  - POSIX signal dispatching (`SIGSTOP`, `SIGCONT`, `SIGTERM`).
  - Nice priority management (`getpriority`/`setpriority`).
  - Structured RFC 4180 CSV auditing.
- **Slide 7: Module 3 — Anomaly Detection & Persistence (Member 3)**
  - 3-consecutive-sample persistence filtering.
  - Active duplicate suppression rule preventing alert floods.
  - Recovery and re-triggering mechanics.
- **Slide 8: Module 3 — Interactive Dashboard & Controller (Member 3)**
  - Terminal ncurses interface with process selection, tree viewing, alert highlighting.
  - Safe human-in-the-loop confirmation for destructive operations.
- **Slide 9: Test Strategy & Verification Results**
  - Automated unit test suites: Anomaly detector (43/43 passed), Resource monitor (passed), Process manager (passed).
  - Synthetic workloads: `cpu_hog` (100% CPU) and `memory_growth` (1 MiB incremental growth up to 64 MiB).
- **Slide 10: Conclusion & Key Engineering Takeaways**
  - Adherence to contract-first architecture and rigid ownership.
  - Zero-heap monitoring loop with deterministic memory footprint.
  - Robust error handling across transient OS process lifecycles.
