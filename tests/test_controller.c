/*
 * tests/test_controller.c
 * Member 3 -- Controller Unit Tests (Phase B Step B2)
 *
 * Implements a test-only stub of Member 2's priority API to test
 * src/controller.c in isolation.
 *
 * Checks:
 *   1. Report-only mode (auto_lower_priority == 0) does not call priority_manager_set.
 *   2. Enabled mode reads current nice value and requests configured increment.
 *   3. Requested nice value is capped at 19.
 *   4. Invalid configuration (nice_increment < 0, NULL pointers) is rejected.
 */

#include "controller.h"
#include "priority_manager.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Priority Manager Test Stubs (Mock)
 * ------------------------------------------------------------------------- */
static int stub_current_nice = 0;
static int stub_set_called   = 0;
static int stub_last_set_val = 0;
static int stub_fail_get     = 0;
static int stub_fail_set     = 0;

int priority_manager_get(pid_t pid, int *nice_value)
{
    (void)pid;
    if (stub_fail_get) {
        return -1;
    }
    if (nice_value == NULL) {
        return -1;
    }
    *nice_value = stub_current_nice;
    return 0;
}

int priority_manager_set(pid_t pid, int nice_value)
{
    (void)pid;
    stub_set_called++;
    stub_last_set_val = nice_value;
    if (stub_fail_set) {
        return -1;
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * Test Framework
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
            printf("  FAIL: %s (line %d)\n", (name), __LINE__);       \
            tests_failed++;                                            \
        }                                                              \
    } while (0)

static void reset_stubs(void)
{
    stub_current_nice = 0;
    stub_set_called   = 0;
    stub_last_set_val = 0;
    stub_fail_get     = 0;
    stub_fail_set     = 0;
}

/* -------------------------------------------------------------------------
 * Test Cases
 * ------------------------------------------------------------------------- */

/* Test 1: Report-only mode (auto_lower_priority == 0) */
static void test_report_only(void)
{
    printf("\n[Test 1: Report-Only Mode]\n");
    reset_stubs();

    AnomalyEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.pid = 1234;
    strncpy(ev.type, "CPU_HIGH", sizeof(ev.type) - 1);

    ControllerConfig cfg;
    cfg.auto_lower_priority = 0;
    cfg.nice_increment      = 5;

    int new_nice = -999;
    char action_result[128] = "";

    int rc = controller_handle_anomaly(&ev, &cfg, &new_nice, action_result);

    CHECK(rc == 0, "Returns 0 in report-only mode");
    CHECK(new_nice == -1, "new_nice_value set to -1 in report-only mode");
    CHECK(stub_set_called == 0, "priority_manager_set was NOT called");
    CHECK(strstr(action_result, "Report-only") != NULL, "action_result mentions Report-only");
}

/* Test 2: Auto-lower enabled mode (normal increment) */
static void test_auto_lower_normal(void)
{
    printf("\n[Test 2: Auto-Lower Enabled Mode (Normal)]\n");
    reset_stubs();
    stub_current_nice = 0;

    AnomalyEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.pid = 2345;
    strncpy(ev.type, "CPU_HIGH", sizeof(ev.type) - 1);

    ControllerConfig cfg;
    cfg.auto_lower_priority = 1;
    cfg.nice_increment      = 5;

    int new_nice = -1;
    char action_result[128] = "";

    int rc = controller_handle_anomaly(&ev, &cfg, &new_nice, action_result);

    CHECK(rc == 0, "Returns 0 on successful priority lower");
    CHECK(stub_set_called == 1, "priority_manager_set was called once");
    CHECK(stub_last_set_val == 5, "Requested nice value is current (0) + increment (5) = 5");
    CHECK(new_nice == 5, "new_nice receives resulting nice value 5");
    CHECK(strstr(action_result, "nice changed") != NULL, "action_result reports nice change");
}

/* Test 3: Auto-lower capped at 19 */
static void test_auto_lower_cap_19(void)
{
    printf("\n[Test 3: Auto-Lower Capped at 19]\n");
    reset_stubs();
    stub_current_nice = 16; /* 16 + 5 = 21, must be capped at 19 */

    AnomalyEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.pid = 3456;
    strncpy(ev.type, "CPU_HIGH", sizeof(ev.type) - 1);

    ControllerConfig cfg;
    cfg.auto_lower_priority = 1;
    cfg.nice_increment      = 5;

    int new_nice = -1;
    char action_result[128] = "";

    int rc = controller_handle_anomaly(&ev, &cfg, &new_nice, action_result);

    CHECK(rc == 0, "Returns 0 when capped at 19");
    CHECK(stub_set_called == 1, "priority_manager_set called");
    CHECK(stub_last_set_val == 19, "Requested nice value capped at 19");
    CHECK(new_nice == 19, "new_nice receives 19");
}

/* Test 4: Invalid configuration & arguments */
static void test_invalid_inputs(void)
{
    printf("\n[Test 4: Invalid Configuration & Arguments]\n");
    reset_stubs();

    AnomalyEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.pid = 4567;

    ControllerConfig cfg;
    cfg.auto_lower_priority = 1;
    cfg.nice_increment      = -1; /* Invalid negative increment */

    int new_nice = -1;
    char action_result[128] = "";

    /* Negative nice_increment */
    int rc = controller_handle_anomaly(&ev, &cfg, &new_nice, action_result);
    CHECK(rc == -1, "Negative nice_increment returns -1");
    CHECK(stub_set_called == 0, "priority setter not called for negative increment");

    /* NULL pointers */
    cfg.nice_increment = 5;
    CHECK(controller_handle_anomaly(NULL, &cfg, &new_nice, action_result) == -1, "NULL anomaly returns -1");
    CHECK(controller_handle_anomaly(&ev, NULL, &new_nice, action_result) == -1, "NULL config returns -1");
    CHECK(controller_handle_anomaly(&ev, &cfg, NULL, action_result) == -1, "NULL new_nice_value returns -1");
    CHECK(controller_handle_anomaly(&ev, &cfg, &new_nice, NULL) == -1, "NULL action_result returns -1");

    /* Failure in priority_manager_get */
    stub_fail_get = 1;
    CHECK(controller_handle_anomaly(&ev, &cfg, &new_nice, action_result) == -1, "Failure in priority get returns -1");
    stub_fail_get = 0;

    /* Failure in priority_manager_set */
    stub_fail_set = 1;
    CHECK(controller_handle_anomaly(&ev, &cfg, &new_nice, action_result) == -1, "Failure in priority set returns -1");
    stub_fail_set = 0;
}

int main(void)
{
    printf("=== Controller Unit Tests (Member 3 Phase B2) ===\n");

    test_report_only();
    test_auto_lower_normal();
    test_auto_lower_cap_19();
    test_invalid_inputs();

    printf("\n=== Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(" (%d FAILED)", tests_failed);
    }
    printf(" ===\n");

    return (tests_failed > 0) ? 1 : 0;
}

