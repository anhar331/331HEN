/* Copyright (C) 2026 OnionHEN / LightningMods
 * Bounded shutdown of a PID whose service identity is checked by its caller.
 */
#pragma once

#include <stdbool.h>
#include <sys/types.h>

typedef enum OnionServiceStopResult {
  ONION_SERVICE_STOP_ABSENT,
  ONION_SERVICE_STOP_STALE,
  ONION_SERVICE_STOPPED,
  ONION_SERVICE_STOP_FAILED,
  ONION_SERVICE_STOP_TIMEOUT,
} OnionServiceStopResult;

typedef struct OnionServiceStopOps {
  bool (*alive)(pid_t pid, void *context);
  bool (*owned)(pid_t pid, const char *expected_name, void *context);
  bool (*terminate)(pid_t pid, void *context);
  void (*wait_ms)(unsigned milliseconds, void *context);
} OnionServiceStopOps;

static inline OnionServiceStopResult onion_service_stop_owned(
    pid_t pid, const char *expected_name, const OnionServiceStopOps *ops,
    void *context, unsigned timeout_ms, unsigned poll_ms) {
  if (pid <= 1)
    return ONION_SERVICE_STOP_ABSENT;
  if (!expected_name || !ops || !ops->alive || !ops->owned ||
      !ops->terminate || !ops->wait_ms || poll_ms == 0)
    return ONION_SERVICE_STOP_FAILED;
  if (!ops->alive(pid, context))
    return ONION_SERVICE_STOP_ABSENT;
  if (!ops->owned(pid, expected_name, context))
    return ONION_SERVICE_STOP_STALE;
  if (!ops->terminate(pid, context))
    return ONION_SERVICE_STOP_FAILED;
  unsigned waited = 0;
  for (;;) {
    /* A reused PID with another name means our old instance has exited. */
    if (!ops->alive(pid, context) ||
        !ops->owned(pid, expected_name, context))
      return ONION_SERVICE_STOPPED;
    if (waited >= timeout_ms)
      return ONION_SERVICE_STOP_TIMEOUT;
    const unsigned remaining = timeout_ms - waited;
    const unsigned interval = remaining < poll_ms ? remaining : poll_ms;
    ops->wait_ms(interval, context);
    waited += interval;
  }
}
