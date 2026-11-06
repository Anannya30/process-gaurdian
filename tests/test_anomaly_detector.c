/*
 * tests/test_anomaly_detector.c
 * Member 3 -- Anomaly Detector Unit Tests
 *
 * Tests the anomaly_detector module in isolation using synthetic
 * ProcessInfo arrays.  Does NOT require Member 1's live process reader.
 *
 * Test cases follow MEMBER_3.md Phase B Step B1 exactly:
 *   1.  CPU below threshold: no event.
 *   2.  One sample above threshold (persistence > 1): no event yet.
 *   3.  Enough consecutive high samples: exactly one CPU event.
 *   4.  Continue supplying high samples: no repeated event.
 *   5.  Below-threshold sample: condition clears.
 *   6.  Enough high samples again: a new event is emitted.
 *   7-8. Same sequence for memory growth.
 *   9.  Newly appearing PID.
 *  10.  PID disappearing and tracker cleanup.
 *  11.  Invalid config, invalid counts and output capacity.
 */

#include "anomaly_detector.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Test infrastructure
 * ------------------------------------------------------------------------- */

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, name)                                              \
    do {                                                               \
        tests_run++;                                                   \
        if (cond) {                                                    \
            printf("  PASS: %s\n", (name));                           \
            tests_passed++;                                            \
        } else {                                                       \
            printf("  FAIL: %s  (line %d)\n", (name), __LINE__);     \
            tests_failed++;                                            \
        }                                                              \
    } while (0)

/* Build a minimal ProcessInfo with just the fields the detector uses. */
static ProcessInfo make_proc(pid_t pid, double cpu, long mem_kb)
{
    ProcessInfo p;
    memset(&p, 0, sizeof(p));
    p.pid              = pid;
    p.ppid             = 1;
    p.state            = 'S';
    p.cpu_usage_percent = cpu;
    p.memory_kb        = mem_kb;
    return p;
}

/* -------------------------------------------------------------------------
 * Test groups
 * ------------------------------------------------------------------------- */

/* ---- CPU anomaly tests (B1 steps 1-7) ---- */
static void test_cpu_anomaly(void)
{
    printf("\n[CPU Anomaly Tests]\n");

    AnomalyConfig cfg;
    cfg.cpu_threshold_percent        = 80.0;
    cfg.memory_growth_threshold_kb   = 99999L; /* High: won't trigger */
    cfg.required_consecutive_samples = 3;

    AnomalyDetector det;
    anomaly_detector_init(&det, &cfg);

    AnomalyEvent events[16];
    int count;

    ProcessInfo procs[1];

    /* 1. CPU well below threshold: no event */
    procs[0] = make_proc(100, 50.0, 1000);
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 0, "CPU below threshold: no event");

    /* 2. One sample above threshold (persistence = 3): still no event */
    procs[0] = make_proc(100, 90.0, 1000);
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 0, "1 high sample: no event (need 3)");

    /* 2b. Second high sample: still no event */
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 0, "2 high samples: no event (need 3)");

    /* 3. Third consecutive high sample: exactly one CPU event */
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 1, "3 consecutive high samples: exactly 1 event emitted");
    if (count >= 1) {
        CHECK(events[0].pid == 100, "CPU event PID is correct");
        CHECK(strcmp(events[0].type, "CPU_HIGH") == 0, "CPU event type is CPU_HIGH");
        CHECK(events[0].measured_value > 80.0, "CPU event measured_value > threshold");
    }

    /* 4. Keep supplying high samples: no repeated event while active */
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 0, "4th high sample: no repeated event (active suppression)");

    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 0, "5th high sample: still no repeated event");

    /* 5. Below-threshold sample: condition clears */
    procs[0] = make_proc(100, 70.0, 1000);
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 0, "Below-threshold clears condition: no immediate event");

    /* 6. Supply 3 high samples again: a NEW event is emitted */
    procs[0] = make_proc(100, 90.0, 1000);
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 0, "1st high after recovery: no event yet");
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 0, "2nd high after recovery: no event yet");
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 1, "3rd high after recovery: new event emitted");
    CHECK(strcmp(events[0].type, "CPU_HIGH") == 0,
          "Re-triggered CPU event type correct");
}

/* ---- Memory growth anomaly tests (B1 steps 8) ---- */
static void test_memory_anomaly(void)
{
    printf("\n[Memory Growth Anomaly Tests]\n");

    AnomalyConfig cfg;
    cfg.cpu_threshold_percent        = 200.0; /* High: won't trigger */
    cfg.memory_growth_threshold_kb   = 1024L; /* 1 MiB per sample    */
    cfg.required_consecutive_samples = 3;

    AnomalyDetector det;
    anomaly_detector_init(&det, &cfg);

    AnomalyEvent events[16];
    int count;

    ProcessInfo procs[1];

    /* First sample sets the baseline; no growth comparison yet. */
    procs[0] = make_proc(200, 0.0, 10000);
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 0, "Memory: first sample sets baseline, no event");

    /* Grow by < threshold: no event */
    procs[0] = make_proc(200, 0.0, 10500); /* +500 KiB < 1024 KiB */
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 0, "Memory growth below threshold: no event");

    /* Grow by >= threshold for 1 sample: not persistent yet */
    procs[0] = make_proc(200, 0.0, 11600); /* +1100 KiB >= 1024 */
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 0, "1 growth sample: no event (need 3 consecutive)");

    procs[0] = make_proc(200, 0.0, 12700); /* +1100 KiB */
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 0, "2 growth samples: no event yet");

    procs[0] = make_proc(200, 0.0, 13800); /* +1100 KiB */
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 1, "3 consecutive growth samples: exactly 1 memory event");
    if (count >= 1) {
        CHECK(events[0].pid == 200, "Memory event PID correct");
        CHECK(strcmp(events[0].type, "MEMORY_GROWTH") == 0,
              "Memory event type is MEMORY_GROWTH");
    }

    /* Continue growing: no repeated event while active */
    procs[0] = make_proc(200, 0.0, 14900);
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 0, "Continued growth: no repeated event (active)");

    /* Growth stops: condition clears */
    procs[0] = make_proc(200, 0.0, 14910); /* +10 KiB < 1024 */
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 0, "Growth stopped: condition clears, no event");

    /* Grow again for 3 samples: new event */
    procs[0] = make_proc(200, 0.0, 16000); /* +1090 KiB >= 1024 */
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 0, "Recovery 1st grow: no event yet");
    procs[0] = make_proc(200, 0.0, 17100);
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 0, "Recovery 2nd grow: no event yet");
    procs[0] = make_proc(200, 0.0, 18200);
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 1, "Recovery 3rd grow: new memory event emitted");
}

/* ---- New PID test (B1 step 9) ---- */
static void test_new_pid(void)
{
    printf("\n[New PID Test]\n");

    AnomalyConfig cfg;
    cfg.cpu_threshold_percent        = 80.0;
    cfg.memory_growth_threshold_kb   = 1024L;
    cfg.required_consecutive_samples = 2;

    AnomalyDetector det;
    anomaly_detector_init(&det, &cfg);

    AnomalyEvent events[16];
    int count;

    /* Start with one process. */
    ProcessInfo procs[2];
    procs[0] = make_proc(300, 90.0, 5000);
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 0, "New PID: 1 high sample, need 2");

    /* Second high sample: event for PID 300 */
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    CHECK(count == 1, "New PID: 2nd high sample emits event");
    CHECK(events[0].pid == 300, "New PID: correct PID in event");

    /* Add a brand-new PID 400 with high CPU; should not trigger immediately */
    procs[1] = make_proc(400, 95.0, 2000);
    count = anomaly_detector_update(&det, procs, 2, events, 16);
    /* PID 300 is still active (suppressed).  PID 400 is new: 1 sample only. */
    CHECK(count == 0,
          "Newly-appearing PID: 1st sample alone does not trigger event");
}

/* ---- PID disappearance and tracker cleanup (B1 step 10) ---- */
static void test_pid_cleanup(void)
{
    printf("\n[PID Cleanup Test]\n");

    AnomalyConfig cfg;
    cfg.cpu_threshold_percent        = 80.0;
    cfg.memory_growth_threshold_kb   = 99999L;
    cfg.required_consecutive_samples = 2;

    AnomalyDetector det;
    anomaly_detector_init(&det, &cfg);

    AnomalyEvent events[16];
    int count;

    /* Establish two processes. */
    ProcessInfo procs[2];
    procs[0] = make_proc(500, 90.0, 1000);
    procs[1] = make_proc(600, 90.0, 2000);
    anomaly_detector_update(&det, procs, 2, events, 16);
    anomaly_detector_update(&det, procs, 2, events, 16);
    /* Both have trackers. */

    int initial_tracker_count = det.tracker_count;
    CHECK(initial_tracker_count == 2, "Cleanup: 2 trackers allocated");

    /* PID 600 disappears. */
    count = anomaly_detector_update(&det, procs, 1, events, 16);
    /* Only PID 500 remains; tracker for 600 must be removed. */
    CHECK(det.tracker_count == 1, "Cleanup: tracker for exited PID removed");
    if (det.tracker_count >= 1) {
        CHECK(det.trackers[0].pid == 500, "Cleanup: remaining tracker is PID 500");
    }
    (void)count; /* Event count not the focus here */
}

/* ---- Invalid arguments and capacity tests (B1 step 11) ---- */
static void test_invalid_args(void)
{
    printf("\n[Invalid Argument Tests]\n");

    AnomalyConfig cfg;
    cfg.cpu_threshold_percent        = 80.0;
    cfg.memory_growth_threshold_kb   = 1024L;
    cfg.required_consecutive_samples = 3;

    AnomalyDetector det;
    anomaly_detector_init(&det, &cfg);

    AnomalyEvent events[16];
    ProcessInfo procs[1];
    procs[0] = make_proc(700, 90.0, 1000);

    /* NULL detector */
    CHECK(anomaly_detector_update(NULL, procs, 1, events, 16) == -1,
          "NULL detector returns -1");

    /* NULL process array */
    CHECK(anomaly_detector_update(&det, NULL, 1, events, 16) == -1,
          "NULL process array returns -1");

    /* NULL event array */
    CHECK(anomaly_detector_update(&det, procs, 1, NULL, 16) == -1,
          "NULL event array returns -1");

    /* Negative process count */
    CHECK(anomaly_detector_update(&det, procs, -1, events, 16) == -1,
          "Negative process_count returns -1");

    /* Negative max_events */
    CHECK(anomaly_detector_update(&det, procs, 1, events, -1) == -1,
          "Negative max_events returns -1");

    /* Zero process count is valid (empty snapshot). */
    int count = anomaly_detector_update(&det, procs, 0, events, 16);
    CHECK(count == 0, "Zero process count: returns 0 events (valid)");

    /* invalid required_consecutive_samples */
    cfg.required_consecutive_samples = 0;
    anomaly_detector_init(&det, &cfg);
    CHECK(anomaly_detector_update(&det, procs, 1, events, 16) == -1,
          "required_consecutive_samples=0 returns -1");

    /* Output capacity: max_events = 0, process is high CPU */
    cfg.required_consecutive_samples = 1;
    cfg.cpu_threshold_percent        = 0.0; /* Triggers immediately */
    anomaly_detector_init(&det, &cfg);
    anomaly_detector_update(&det, procs, 1, events, 1);
    /* Second call with max_events=0 must not write any event. */
    /* Reset to get a fresh event opportunity */
    anomaly_detector_init(&det, &cfg);
    count = anomaly_detector_update(&det, procs, 1, events, 0);
    CHECK(count == 0, "max_events=0: no events written, returns 0");
}

/* ---- NULL config in init (safe no-op) ---- */
static void test_init_null(void)
{
    printf("\n[Init NULL Safety Tests]\n");

    AnomalyDetector det;
    memset(&det, 0xAB, sizeof(det)); /* Fill with junk */

    /* Should not crash with NULL arguments. */
    anomaly_detector_init(NULL, NULL);
    CHECK(1, "anomaly_detector_init(NULL, NULL) does not crash");

    AnomalyConfig cfg;
    cfg.cpu_threshold_percent        = 80.0;
    cfg.memory_growth_threshold_kb   = 1024L;
    cfg.required_consecutive_samples = 3;
    anomaly_detector_init(NULL, &cfg);
    CHECK(1, "anomaly_detector_init(NULL, &cfg) does not crash");
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
    printf("=== Anomaly Detector Unit Tests (Member 3 Phase B1) ===\n");

    test_cpu_anomaly();
    test_memory_anomaly();
    test_new_pid();
    test_pid_cleanup();
    test_invalid_args();
    test_init_null();

    printf("\n=== Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf("  (%d FAILED)", tests_failed);
    }
    printf(" ===\n");

    return (tests_failed > 0) ? 1 : 0;
}
