/* Host tests for PID-bound, serialized Toolbox inject. */
#include "test_harness.h"
#include "toolbox_injection.hpp"

#include <atomic>
#include <thread>

using onion::ToolboxInject;
using onion::ToolboxInjectionResult;

namespace {

void reset_toolbox_marker() {
  onion_ready_clear(ONION_READY_TOOLBOX);
  onion_ready_clear(ONION_FLAG_TOOLBOX_ATTEMPT);
}

int test_injects_and_publishes_current_pid() {
  reset_toolbox_marker();
  ToolboxInject injector;
  int inject_count = 0;
  constexpr pid_t kPid = 4100;

  const auto outcome = injector.inject(
      []() -> pid_t { return kPid; },
      [&](pid_t pid) -> bool {
        ++inject_count;
        return onion_ready_signal_pid(ONION_READY_TOOLBOX, pid);
      },
      100, 50);

  TEST_ASSERT_TRUE(outcome.result == ToolboxInjectionResult::Injected);
  TEST_ASSERT_EQ_INT((int)kPid, (int)outcome.pid);
  TEST_ASSERT_EQ_INT(1, inject_count);
  TEST_ASSERT_TRUE(injector.is_ready_for(kPid));
  reset_toolbox_marker();
  return 0;
}

int test_same_pid_skips_duplicate_injection() {
  reset_toolbox_marker();
  ToolboxInject injector;
  constexpr pid_t kPid = 4200;
  TEST_ASSERT_TRUE(onion_ready_signal_pid(ONION_READY_TOOLBOX, kPid));

  int inject_count = 0;
  const auto outcome = injector.inject(
      []() -> pid_t { return kPid; },
      [&](pid_t) -> bool {
        ++inject_count;
        return true;
      },
      100, 50);

  TEST_ASSERT_TRUE(outcome.result == ToolboxInjectionResult::AlreadyReady);
  TEST_ASSERT_EQ_INT(0, inject_count);
  reset_toolbox_marker();
  return 0;
}

int test_new_pid_replaces_stale_marker() {
  reset_toolbox_marker();
  ToolboxInject injector;
  constexpr pid_t kOldPid = 4300;
  constexpr pid_t kNewPid = 4301;
  TEST_ASSERT_TRUE(onion_ready_signal_pid(ONION_READY_TOOLBOX, kOldPid));

  int inject_count = 0;
  const auto outcome = injector.inject(
      []() -> pid_t { return kNewPid; },
      [&](pid_t pid) -> bool {
        ++inject_count;
        return onion_ready_signal_pid(ONION_READY_TOOLBOX, pid);
      },
      100, 50);

  TEST_ASSERT_TRUE(outcome.result == ToolboxInjectionResult::Injected);
  TEST_ASSERT_EQ_INT(1, inject_count);
  TEST_ASSERT_TRUE(!injector.is_ready_for(kOldPid));
  TEST_ASSERT_TRUE(injector.is_ready_for(kNewPid));
  reset_toolbox_marker();
  return 0;
}

int test_expected_pid_overrides_resolver() {
  reset_toolbox_marker();
  ToolboxInject injector;
  constexpr pid_t kExpectedPid = 4351;
  int resolved = 0;
  int injected_pid = 0;

  const auto outcome = injector.inject(
      [&]() -> pid_t {
        ++resolved;
        return resolved == 1 ? 0 : kExpectedPid;
      },
      [&](pid_t pid) -> bool {
        injected_pid = static_cast<int>(pid);
        return onion_ready_signal_pid(ONION_READY_TOOLBOX, pid);
      },
      100, 50, kExpectedPid);

  TEST_ASSERT_TRUE(outcome.result == ToolboxInjectionResult::Injected);
  TEST_ASSERT_EQ_INT((int)kExpectedPid, injected_pid);
  TEST_ASSERT_EQ_INT((int)kExpectedPid, (int)outcome.pid);
  TEST_ASSERT_EQ_INT(2, resolved);
  reset_toolbox_marker();
  return 0;
}

int test_failed_injection_blocks_same_pid_across_daemon_restart() {
  reset_toolbox_marker();
  ToolboxInject injector;
  constexpr pid_t kPid = 4400;

  auto failed = injector.inject(
      []() -> pid_t { return kPid; }, [](pid_t) -> bool { return false; }, 100,
      50);
  TEST_ASSERT_TRUE(failed.result == ToolboxInjectionResult::InjectFailed);
  TEST_ASSERT_TRUE(!onion_ready_is_set(ONION_READY_TOOLBOX));
  TEST_ASSERT_TRUE(onion_ready_matches_pid(ONION_FLAG_TOOLBOX_ATTEMPT, kPid));

  ToolboxInject restarted_daemon;
  int inject_count = 0;
  const auto retry = restarted_daemon.inject(
      []() -> pid_t { return kPid; },
      [&](pid_t) -> bool { ++inject_count; return true; }, 100, 50);
  TEST_ASSERT_TRUE(retry.result == ToolboxInjectionResult::RestartRequired);
  TEST_ASSERT_EQ_INT(0, inject_count);
  reset_toolbox_marker();
  return 0;
}

int test_timeout_blocks_retry_and_accepts_late_ready() {
  reset_toolbox_marker();
  ToolboxInject injector;
  constexpr pid_t kPid = 4401;
  int inject_count = 0;
  auto inject = [&](pid_t) -> bool { ++inject_count; return true; };
  const auto timed_out = injector.inject(
      []() -> pid_t { return kPid; }, inject, 100, 50);
  TEST_ASSERT_TRUE(timed_out.result == ToolboxInjectionResult::ReadyTimeout);
  TEST_ASSERT_TRUE(!onion_ready_is_set(ONION_READY_TOOLBOX));
  TEST_ASSERT_TRUE(onion_ready_matches_pid(ONION_FLAG_TOOLBOX_ATTEMPT, kPid));
  TEST_ASSERT_TRUE(injector.inject([]() -> pid_t { return kPid; }, inject,
                                    100, 50).result ==
                   ToolboxInjectionResult::RestartRequired);
  TEST_ASSERT_EQ_INT(1, inject_count);
  TEST_ASSERT_TRUE(onion_ready_signal_pid(ONION_READY_TOOLBOX, kPid));
  TEST_ASSERT_TRUE(injector.inject([]() -> pid_t { return kPid; }, inject,
                                    100, 50).result ==
                   ToolboxInjectionResult::AlreadyReady);
  TEST_ASSERT_EQ_INT(1, inject_count);
  reset_toolbox_marker();
  return 0;
}

int test_new_pid_recovers_after_failed_initialization() {
  reset_toolbox_marker();
  ToolboxInject injector;
  constexpr pid_t kOldPid = 4402, kNewPid = 4403;
  TEST_ASSERT_TRUE(injector.inject(
      []() -> pid_t { return kOldPid; }, [](pid_t) -> bool { return false; },
      100, 50).result == ToolboxInjectionResult::InjectFailed);
  int inject_count = 0;
  const auto recovered = injector.inject(
      []() -> pid_t { return kNewPid; },
      [&](pid_t pid) -> bool {
        ++inject_count;
        return onion_ready_signal_pid(ONION_READY_TOOLBOX, pid);
      }, 100, 50);
  TEST_ASSERT_TRUE(recovered.result == ToolboxInjectionResult::Injected);
  TEST_ASSERT_EQ_INT(1, inject_count);
  TEST_ASSERT_TRUE(onion_ready_matches_pid(ONION_FLAG_TOOLBOX_ATTEMPT, kNewPid));
  reset_toolbox_marker();
  return 0;
}

int test_missing_ready_marker_does_not_reinject_live_hooks() {
  reset_toolbox_marker();
  ToolboxInject injector;
  constexpr pid_t kPid = 4404;
  int inject_count = 0;
  const auto first = injector.inject(
      []() -> pid_t { return kPid; },
      [&](pid_t pid) -> bool {
        ++inject_count;
        return onion_ready_signal_pid(ONION_READY_TOOLBOX, pid);
      }, 100, 50);
  TEST_ASSERT_TRUE(first.result == ToolboxInjectionResult::Injected);
  onion_ready_clear(ONION_READY_TOOLBOX);
  ToolboxInject restarted_daemon;
  const auto second = restarted_daemon.inject(
      []() -> pid_t { return kPid; },
      [&](pid_t) -> bool { ++inject_count; return true; }, 100, 50);
  TEST_ASSERT_TRUE(second.result == ToolboxInjectionResult::RestartRequired);
  TEST_ASSERT_EQ_INT(1, inject_count);
  reset_toolbox_marker();
  return 0;
}

int test_concurrent_requests_inject_once() {
  reset_toolbox_marker();
  ToolboxInject injector;
  constexpr pid_t kPid = 4500;
  std::atomic<int> inject_count{0};
  std::atomic<int> ready_count{0};

  auto request = [&]() {
    const auto outcome = injector.inject(
        []() -> pid_t { return kPid; },
        [&](pid_t pid) -> bool {
          ++inject_count;
          return onion_ready_signal_pid(ONION_READY_TOOLBOX, pid);
        },
        100, 50);
    if (outcome.ready()) {
      ++ready_count;
    }
  };

  std::thread first(request);
  std::thread second(request);
  first.join();
  second.join();

  TEST_ASSERT_EQ_INT(1, inject_count.load());
  TEST_ASSERT_EQ_INT(2, ready_count.load());
  reset_toolbox_marker();
  return 0;
}

} // namespace

extern "C" int test_toolbox_injection_suite(void) {
  int failures = 0;
  failures += onion_test_run("toolbox_injection.inject",
                             test_injects_and_publishes_current_pid);
  failures += onion_test_run("toolbox_injection.same_pid_skip",
                             test_same_pid_skips_duplicate_injection);
  failures += onion_test_run("toolbox_injection.new_pid",
                             test_new_pid_replaces_stale_marker);
  failures += onion_test_run("toolbox_injection.expected_pid",
                             test_expected_pid_overrides_resolver);
  failures += onion_test_run("toolbox_injection.failed_attempt_persists",
                             test_failed_injection_blocks_same_pid_across_daemon_restart);
  failures += onion_test_run("toolbox_injection.timeout_late_ready",
                             test_timeout_blocks_retry_and_accepts_late_ready);
  failures += onion_test_run("toolbox_injection.failed_new_pid_recovery",
                             test_new_pid_recovers_after_failed_initialization);
  failures += onion_test_run("toolbox_injection.lost_ready_no_reinject",
                             test_missing_ready_marker_does_not_reinject_live_hooks);
  failures += onion_test_run("toolbox_injection.serialized",
                             test_concurrent_requests_inject_once);
  return failures;
}
