# Member 3 --- Anomaly Detection, Adaptive Controller, Dashboard and Integration

**Branch:** `feature/detection-ui`\
**Owned implementation:** Anomaly detection, controller policy, ncurses
dashboard and `main.c` integration\
**Read first:** `MASTER_SPEC.md` in full

## 1. Your responsibility

Implement the modules that answer:

-   Is a process's CPU usage persistently too high?
-   Is a process's resident memory persistently growing?
-   Has an anomaly met the required persistence rule?
-   Should the Guardian only report the anomaly or lower the process
    nice priority?
-   How does the user view processes, tree, alerts and logs?
-   How are all completed modules connected into one executable?

You own the detector, controller, dashboard and main integration. You do
not implement process discovery, process tree, state tracker, signals,
priority APIs or logger.

## 2. Exact file ownership

You may create/edit:

-   `include/anomaly_detector.h`
-   `include/controller.h`
-   `include/dashboard.h`
-   `src/anomaly_detector.c`
-   `src/controller.c`
-   `src/dashboard.c`
-   `src/main.c`
-   `tests/test_anomaly_detector.c`

You coordinate the integration build and Makefile only through an agreed
shared change. Do not independently rewrite another member's modules.

## 3. Required public APIs

Implement exactly these declarations from `MASTER_SPEC.md`:

``` c
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

int controller_handle_anomaly(
    const AnomalyEvent *anomaly,
    const ControllerConfig *config,
    int *new_nice_value,
    char action_result[128]
);

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
```

Do not change names, parameters, return types or semantics.

## 4. Implementation sequence

### Phase A --- Independent implementation

#### Step A1: Inspect repository and shared interfaces

1.  Read `MASTER_SPEC.md`.
2.  Run `git status`.
3.  Confirm you are on `feature/detection-ui`.
4.  Inspect all shared headers.
5.  Confirm exact `ProcessInfo`, `ProcessTreeNode`, `GuardianEvent` and
    API declarations.
6.  Do not modify another member's headers or implementation files.
7.  If an interface is missing or inconsistent, stop and ask before
    proceeding.

#### Step A2: Implement anomaly detector

1.  Create `include/anomaly_detector.h` with the exact structures and
    declarations.
2.  Create `src/anomaly_detector.c`.
3.  Implement `anomaly_detector_init`:
    -   Validate pointers.
    -   Copy the supplied configuration into the detector.
    -   Set tracker count to zero.
    -   Initialize tracker entries safely.
4.  Implement `anomaly_detector_update`:
    -   Validate detector, process array, event array and counts.
    -   For each current process, find or allocate its tracker by PID.
    -   If CPU usage is strictly greater than the configured threshold,
        increment `cpu_high_count`; otherwise reset it to zero.
    -   If current memory is greater than previous memory by at least
        the configured growth threshold, increment
        `memory_growth_count`; otherwise reset it to zero.
    -   Update `previous_memory_kb` after evaluating growth.
    -   Emit a CPU anomaly once the CPU counter reaches the required
        consecutive sample count.
    -   Emit a memory anomaly once the memory counter reaches the
        required consecutive sample count.
    -   Do not emit the same continuously active anomaly on every later
        update.
    -   Track whether each condition is currently active so it can be
        emitted again after clearing and later recurring. If the agreed
        tracker structure does not contain enough fields for this, stop
        and request an approved shared-interface update; do not add
        fields unilaterally.
    -   Remove/recycle trackers for PIDs absent from the current process
        list.
    -   Never exceed `MAX_PROCESSES` trackers or `max_events` output
        records.
5.  Return the number of generated events; return `-1` for invalid
    arguments.
6.  Do not call the logger, controller, dashboard, signals or priority
    APIs from the detector.

#### Step A3: Implement adaptive controller

1.  Create `include/controller.h` with the exact `ControllerConfig` and
    function declaration.
2.  Create `src/controller.c`.
3.  Validate all pointers and configuration values.
4.  If `auto_lower_priority == 0`, do not call any priority API. Set
    `new_nice_value` to `-1` and set `action_result` to a clear
    report-only message.
5.  If `auto_lower_priority == 1`, require `nice_increment >= 0`.
6.  Call Member 2's `priority_manager_get` for the anomaly PID.
7.  Calculate requested nice value as current nice value plus
    `nice_increment`.
8.  Cap the requested value at 19.
9.  Call Member 2's `priority_manager_set`.
10. Store the resulting nice value and a clear success/failure message.
11. Never call stop, resume or terminate.
12. Do not log directly. `main.c` creates the final `GuardianEvent` and
    calls Member 2's logger.

#### Step A4: Implement ncurses dashboard

1.  Create `include/dashboard.h` with exact declarations.
2.  Create `src/dashboard.c`.
3.  Implement `dashboard_init`:
    -   Call `initscr`.
    -   Enable cbreak and noecho.
    -   Enable keypad input.
    -   Configure non-blocking or timed input so the monitor loop
        continues.
    -   Initialize color pairs only if the terminal supports colors.
    -   Return a failure code if initialization fails.
4.  Implement `dashboard_shutdown`:
    -   Restore terminal state with `endwin`.
    -   Make repeated shutdown safe.
5.  Implement `dashboard_render`:
    -   Clear/redraw the screen safely.
    -   Display application title and monitoring status.
    -   Display a process table with PID, PPID, name, state, CPU%,
        memory and priority.
    -   Highlight the selected row.
    -   Display either process list or tree view according to current
        view mode.
    -   Display recent anomaly alerts.
    -   Display a short key-help line.
    -   Handle empty arrays and terminal sizes smaller than the full
        layout.
    -   Do not read beyond supplied counts.
6.  Implement selection:
    -   Up/down arrows move selection.
    -   Keep selected index valid when process count changes.
    -   `dashboard_get_selected_pid` returns selected PID or `-1`.
7.  Implement key handling:
    -   `q`: request quit.
    -   `t`: toggle process/tree view.
    -   `s`: request stop of selected PID.
    -   `c`: request continue of selected PID.
    -   `k`: request termination; `main.c` must confirm before calling
        SIGTERM.
    -   `p`: request nice-value change; `main.c` obtains and validates
        value.
    -   `l`: toggle recent event/log view if available.
8.  Dashboard must not call `kill`, `setpriority`, `getpriority`, or
    logger functions directly.
9.  Do not perform process enumeration or anomaly calculations in the
    dashboard.
10. Ensure `dashboard_shutdown` is called on normal quit and handled
    error paths.

#### Step A5: Implement `main.c` integration

Only after module-level work and independent tests are complete,
implement `src/main.c`.

1.  Include only the required module headers.
2.  Define configuration values exactly as in `MASTER_SPEC.md`:
    -   sample interval: `DEFAULT_SAMPLE_INTERVAL_MS`
    -   CPU threshold: `80.0`
    -   memory growth threshold: `10240` KiB per sample
    -   required consecutive samples: `3`
    -   automatic priority lowering: `0` (disabled)
    -   nice increment: `5`
3.  Allocate separate current and previous `ProcessInfo` arrays of
    `MAX_PROCESSES`.
4.  Allocate tree-node and event arrays using their documented maximum
    sizes.
5.  Initialize logger at `logs/process_guardian.csv`.
6.  Initialize dashboard.
7.  Collect an initial process snapshot.
8.  Enter the monitoring loop:
    -   Capture monotonic time.
    -   Collect current process records.
    -   Calculate elapsed time.
    -   Update CPU usage using Member 1's resource monitor.
    -   Build process tree using Member 2's process tree module.
    -   Detect state changes using Member 2's state tracker.
    -   Convert state changes to `GuardianEvent` and log them.
    -   Run anomaly detector.
    -   Pass each anomaly to the controller.
    -   Convert each anomaly and controller result to `GuardianEvent`
        and log it.
    -   Render the dashboard.
    -   Read a key without blocking the next sample.
    -   For `s`, call Member 2's stop API for the selected PID.
    -   For `c`, call Member 2's resume API.
    -   For `k`, ask for confirmation, then call Member 2's terminate
        API only after confirmation.
    -   For `p`, prompt for a nice value, validate \[-20, 19\], then
        call Member 2's priority APIs.
    -   Log each user-requested operation and its result.
    -   Wait for the configured interval without busy-spinning.
    -   Copy current snapshot to previous snapshot for the next
        iteration.
9.  Handle process disappearance and permission errors without crashing.
10. On exit, close logger and restore terminal state.
11. Do not duplicate the logic of any module inside `main.c`.

### Phase B --- Independent testing

Before full integration, test your modules independently.

#### Step B1: Anomaly detector tests

Use synthetic `ProcessInfo` arrays. Do not require Member 1's live
process reader for these tests.

1.  Configure CPU threshold and persistence count.
2.  Supply CPU below threshold; expect no event.
3.  Supply one sample above threshold; expect no event if persistence is
    greater than one.
4.  Supply enough consecutive high samples; expect exactly one CPU
    event.
5.  Keep supplying high samples; expect no repeated event for the same
    active period.
6.  Supply a below-threshold sample to clear the condition.
7.  Supply enough high samples again; expect a new event.
8.  Repeat equivalent tests for memory growth.
9.  Test a newly appearing PID.
10. Test a PID disappearing and tracker cleanup.
11. Test invalid config, invalid counts and output capacity.
12. Verify no out-of-bounds access under AddressSanitizer if available.

#### Step B2: Controller tests

Because Member 2's priority module may not yet be complete:

1.  Create a temporary test stub implementing the exact priority API in
    a separate test-only file.
2.  Verify report-only mode does not call the stub setter.
3.  Verify enabled mode reads current nice value and requests the
    configured increment.
4.  Verify the requested nice value is capped at 19.
5.  Verify invalid configuration is rejected.
6.  Do not edit Member 2's actual source files to create the stub.

#### Step B3: Dashboard tests

1.  Compile and run dashboard with synthetic process arrays.
2.  Test empty process list.
3.  Test process list view.
4.  Test tree view.
5.  Test arrow-key selection.
6.  Test selected PID retrieval.
7.  Test each key binding.
8.  Test terminal resize and small terminal dimensions.
9.  Verify dashboard exits with terminal settings restored.

#### Step B4: Build checks

Compile your independent modules with the required include paths and
warning flags. For dashboard, link ncurses. If other modules are
unfinished, use temporary test stubs in a separate test-only location.
Do not change shared APIs to make a temporary test compile.

### Phase C --- Integration

1.  Confirm Member 1 and Member 2 have completed their module tests and
    merged approved pull requests.
2.  Pull the latest `main`.
3.  Verify every header against `MASTER_SPEC.md`.
4.  Update the shared Makefile only with team agreement.
5.  Link all source files and ncurses into the Guardian executable.
6.  Resolve compile errors within your owned files first.
7.  If an error originates in another member's module, report it to that
    member; do not rewrite their implementation.
8.  Run the full application.
9.  Verify process table, tree, CPU sampling, memory sampling, state
    changes, anomalies, controller and logs.
10. Test stop/resume/terminate only on project-owned test processes.
11. Test automatic priority lowering only after explicit team approval;
    keep it disabled by default.
12. Run the final demonstration sequence in `MASTER_SPEC.md`.
13. Record all commands and test results in the project README.
14. Ask the team to review the complete integration before merging.

## 5. Completion checklist

-   [ ] Detector and controller headers/source files exist.
-   [ ] Exact APIs and structures implemented.
-   [ ] Persistent CPU and memory-growth rules work.
-   [ ] Repeated anomaly events are suppressed while active.
-   [ ] Controller defaults to report-only.
-   [ ] Automatic controller never sends stop/resume/terminate signals.
-   [ ] ncurses dashboard displays process data, tree and alerts.
-   [ ] Key input is handled by main integration, not by direct system
    calls in the UI.
-   [ ] Main loop uses the other members' APIs without duplicating their
    logic.
-   [ ] Logger and terminal are closed/restored on exit.
-   [ ] Independent tests pass.
-   [ ] Full application builds and runs after integration.
-   [ ] No other member's files were modified without approval.
-   [ ] Changes pushed on `feature/detection-ui` with a pull request.
