/* Host tests for owned-service shutdown before bootstrapper replacement. */
#include "test_harness.h"
#include <onion/service_stop.h>
#include <string.h>

typedef struct StopMock {
  bool alive;
  bool owned;
  bool terminate_ok;
  unsigned exit_after_ms;
  unsigned elapsed_ms;
  unsigned terminate_calls;
  unsigned wait_calls;
} StopMock;

static bool mock_alive(pid_t pid, void *context) {
  StopMock *mock = context;
  return pid == 5000 && mock->alive;
}

static bool mock_owned(pid_t pid, const char *expected, void *context) {
  StopMock *mock = context;
  return pid == 5000 && strcmp(expected, "onion_util.elf") == 0 && mock->owned;
}

static bool mock_terminate(pid_t pid, void *context) {
  StopMock *mock = context;
  if (pid != 5000)
    return false;
  ++mock->terminate_calls;
  return mock->terminate_ok;
}

static void mock_wait(unsigned milliseconds, void *context) {
  StopMock *mock = context;
  ++mock->wait_calls;
  mock->elapsed_ms += milliseconds;
  if (mock->exit_after_ms && mock->elapsed_ms >= mock->exit_after_ms)
    mock->alive = false;
}

static const OnionServiceStopOps kOps = {
    mock_alive, mock_owned, mock_terminate, mock_wait};

static int test_waits_for_delayed_exit_before_replacement(void) {
  StopMock mock = {.alive = true, .owned = true, .terminate_ok = true,
                   .exit_after_ms = 250};
  TEST_ASSERT_TRUE(onion_service_stop_owned(5000, "onion_util.elf", &kOps,
                                            &mock, 3000, 100) ==
                   ONION_SERVICE_STOPPED);
  TEST_ASSERT_EQ_INT(1, mock.terminate_calls);
  TEST_ASSERT_EQ_INT(3, mock.wait_calls);
  TEST_ASSERT_EQ_INT(300, mock.elapsed_ms);
  TEST_ASSERT_TRUE(!mock.alive);
  return 0;
}

static int test_termination_failure_refuses_replacement(void) {
  StopMock mock = {.alive = true, .owned = true};
  TEST_ASSERT_TRUE(onion_service_stop_owned(5000, "onion_util.elf", &kOps,
                                            &mock, 3000, 100) ==
                   ONION_SERVICE_STOP_FAILED);
  TEST_ASSERT_EQ_INT(1, mock.terminate_calls);
  TEST_ASSERT_EQ_INT(0, mock.wait_calls);
  TEST_ASSERT_TRUE(mock.alive);
  return 0;
}

static int test_exit_timeout_is_bounded_and_refuses_replacement(void) {
  StopMock mock = {.alive = true, .owned = true, .terminate_ok = true};
  TEST_ASSERT_TRUE(onion_service_stop_owned(5000, "onion_util.elf", &kOps,
                                            &mock, 250, 100) ==
                   ONION_SERVICE_STOP_TIMEOUT);
  TEST_ASSERT_EQ_INT(1, mock.terminate_calls);
  TEST_ASSERT_EQ_INT(3, mock.wait_calls);
  TEST_ASSERT_EQ_INT(250, mock.elapsed_ms);
  TEST_ASSERT_TRUE(mock.alive);
  return 0;
}

static int test_stale_foreign_pid_is_never_killed(void) {
  StopMock mock = {.alive = true};
  TEST_ASSERT_TRUE(onion_service_stop_owned(5000, "onion_util.elf", &kOps,
                                            &mock, 3000, 100) ==
                   ONION_SERVICE_STOP_STALE);
  TEST_ASSERT_EQ_INT(0, mock.terminate_calls);
  TEST_ASSERT_EQ_INT(0, mock.wait_calls);
  return 0;
}

static int test_absent_or_system_pid_is_never_killed(void) {
  StopMock mock = {0};
  TEST_ASSERT_TRUE(onion_service_stop_owned(5000, "onion_util.elf", &kOps,
                                            &mock, 3000, 100) ==
                   ONION_SERVICE_STOP_ABSENT);
  TEST_ASSERT_TRUE(onion_service_stop_owned(1, "onion_util.elf", &kOps,
                                            &mock, 3000, 100) ==
                   ONION_SERVICE_STOP_ABSENT);
  TEST_ASSERT_EQ_INT(0, mock.terminate_calls);
  TEST_ASSERT_EQ_INT(0, mock.wait_calls);
  return 0;
}

int test_service_stop_suite(void) {
  int failures = 0;
  failures += onion_test_run("service_stop.delayed_exit",
                             test_waits_for_delayed_exit_before_replacement);
  failures += onion_test_run("service_stop.termination_failure",
                             test_termination_failure_refuses_replacement);
  failures += onion_test_run("service_stop.bounded_timeout",
                             test_exit_timeout_is_bounded_and_refuses_replacement);
  failures += onion_test_run("service_stop.foreign_pid",
                             test_stale_foreign_pid_is_never_killed);
  failures += onion_test_run("service_stop.absent_system_pid",
                             test_absent_or_system_pid_is_never_killed);
  return failures;
}
