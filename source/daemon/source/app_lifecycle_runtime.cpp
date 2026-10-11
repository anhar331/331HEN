/* Copyright (C) 2026 OnionHEN / LightningMods */

#include "app_lifecycle_runtime.hpp"
#include "daemon_ops.hpp"
#include "sprx_plugin_manager_runtime.hpp"

#include <onion/app_lifecycle.hpp>
#include <onion/log.h>
#include <onion/ipc_client.hpp>
#include "onion_cjson.hpp"
#include "freebsd-helper.h"
#include <cstring>
#include <map>
#include <sys/sysctl.h>
#include <vector>

extern "C" int sceKernelGetAppInfo(pid_t, void *);
extern "C" int sceShellCoreUtilGetAppLaunchedParamString(uint32_t, const char *, char *, size_t);

namespace onion::daemon::app_lifecycle {
namespace {

class AppJailbreakLifecycleSubscriber final
    : public onion::lifecycle::ISubscriber {
public:
  int priority() const noexcept override { return 10; }

  bool on_event(const onion::lifecycle::Event &event) noexcept override {
    const char *title_id = event.app.title_id.c_str();
    bool completed = false;
    switch (event.type) {
    case onion::lifecycle::EventType::BigAppStarted:
      completed = app_jailbreak_on_big_app_started(
          event.app.pid, event.app.app_id, title_id);
      break;
    case onion::lifecycle::EventType::BigAppExited:
      completed = app_jailbreak_on_big_app_exited(
          event.app.pid, event.app.app_id, title_id);
      break;
    }
    if (!completed) {
      LOG_WARN("[lifecycle] AppJailbreak did not acknowledge event type=%u "
               "pid=%d tid=%s",
               static_cast<unsigned>(event.type), static_cast<int>(event.app.pid),
               title_id);
    }
    return completed;
  }
};

class SprxLifecycleSubscriber final : public onion::lifecycle::ISubscriber {
public:
  int priority() const noexcept override { return 100; }

  bool on_event(const onion::lifecycle::Event &event) noexcept override {
    switch (event.type) {
    case onion::lifecycle::EventType::BigAppStarted:
      sprx_plugins::on_big_app_started(event.app.pid, event.app.app_id,
                                       event.app.title_id);
      break;
    case onion::lifecycle::EventType::BigAppExited:
      sprx_plugins::on_big_app_exited(event.app.pid, event.app.app_id,
                                      event.app.title_id);
      break;
    }
    return true;
  }
};

uint64_t process_generation(pid_t pid) {
  int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(pid)};
  size_t size = 0;
  if (sysctl(mib, 4, nullptr, &size, nullptr, 0) != 0 || size < sizeof(kinfo_proc) || size > 16 * 1024 * 1024) return 0;
  std::vector<char> data(size);
  if (sysctl(mib, 4, data.data(), &size, nullptr, 0) != 0 || size < sizeof(kinfo_proc) || size > data.size()) return 0;
  const auto *process = reinterpret_cast<const kinfo_proc *>(data.data());
  if (process->ki_pid != pid || process->ki_structsize < static_cast<int>(sizeof(kinfo_proc)) || static_cast<size_t>(process->ki_structsize) > size || process->ki_start.tv_sec <= 0 || process->ki_start.tv_usec < 0 || process->ki_start.tv_usec >= 1000000) return 0;
  return (static_cast<uint64_t>(process->ki_start.tv_sec) << 32) | static_cast<uint64_t>(process->ki_start.tv_usec);
}
class GamePatchSubscriber final : public onion::lifecycle::ISubscriber {
  std::map<pid_t, uint64_t> generations_;
public:
  int priority() const noexcept override { return 20; }
  bool on_event(const onion::lifecycle::Event &event) noexcept override {
    try {
      cJSON *j = cJSON_CreateObject();
      if (!j) return true;
      cJSON_AddNumberToObject(j, "pid", event.app.pid);
      if (event.type == onion::lifecycle::EventType::BigAppExited) {
        auto old = generations_.find(event.app.pid);
        if (old == generations_.end()) { cJSON_Delete(j); return true; }
        cJSON_AddStringToObject(j, "action", "exit");
        cJSON_AddStringToObject(j, "generation", std::to_string(old->second).c_str());
        generations_.erase(old);
      } else {
        const uint64_t generation = process_generation(event.app.pid);
        unsigned char app_info[128]{};
        uint32_t appid = 0;
        if (!generation || sceKernelGetAppInfo(event.app.pid, app_info) != 0) { cJSON_Delete(j); return true; }
        std::memcpy(&appid, app_info, sizeof(appid));
        char title[15]{};
        std::memcpy(title, app_info + 16, 14);
        if (appid != event.app.app_id || event.app.title_id != title) { cJSON_Delete(j); return true; }
        char live_title[16]{};
        if (sceShellCoreUtilGetAppLaunchedParamString(appid, "TITLE_ID", live_title, sizeof(live_title)) >= 0 && live_title[0] && event.app.title_id != live_title) { cJSON_Delete(j); return true; }
        char version[32]{};
        if (sceShellCoreUtilGetAppLaunchedParamString(appid, "APP_VER", version, sizeof(version)) < 0 || !version[0]) {
          version[0] = 0;
          if (sceShellCoreUtilGetAppLaunchedParamString(appid, "CONTENT_VERSION", version, sizeof(version)) < 0) version[0] = 0;
        }
        version[sizeof(version) - 1] = 0;
        cJSON_AddStringToObject(j, "action", "launch");
        cJSON_AddNumberToObject(j, "appid", appid);
        // JSON doubles lose bits of process-generation IDs: encode as decimal.
        cJSON_AddStringToObject(j, "generation", std::to_string(generation).c_str());
        cJSON_AddStringToObject(j, "title", event.app.title_id.c_str());
        cJSON_AddStringToObject(j, "version", version);
        generations_[event.app.pid] = generation;
      }
      std::string reply;
      if (!IPC_Client::getInstance(true).IPCSendCommand(BREW_UTIL_PATCH_LAUNCH, reply, onion_cjson::print_owned(j)))
        LOG_WARN("[patches] lifecycle snapshot unavailable; saved selections retained");
    } catch (...) {
      LOG_WARN("[patches] lifecycle snapshot failed; other launch subscribers continue");
    }
    // Optional patch errors must never suppress the existing SPRX lifecycle.
    return true;
  }
};

struct Runtime {
  AppJailbreakLifecycleSubscriber app_jailbreak;
  SprxLifecycleSubscriber sprx;
  GamePatchSubscriber patches;
  onion::lifecycle::Dispatcher dispatcher;

  Runtime() : dispatcher(32) {
    (void)dispatcher.subscribe(app_jailbreak);
    (void)dispatcher.subscribe(sprx);
    (void)dispatcher.subscribe(patches);
  }
};

Runtime &runtime() {
  static Runtime value;
  return value;
}

bool publish(onion::lifecycle::EventType type, pid_t pid, uint32_t app_id,
             std::string_view title_id) {
  onion::lifecycle::Event event;
  event.type = type;
  event.app.pid = pid;
  event.app.app_id = app_id;
  event.app.title_id = title_id;
  if (!runtime().dispatcher.publish(std::move(event))) {
    LOG_WARN("[lifecycle] dropped event type=%u pid=%d tid=%.*s",
             static_cast<unsigned>(type), static_cast<int>(pid),
             static_cast<int>(title_id.size()), title_id.data());
    return false;
  }
  return true;
}

} // namespace

bool start() {
  const bool started = runtime().dispatcher.start();
  if (!started)
    LOG_ERROR("[lifecycle] dispatcher start failed");
  return started;
}

void stop() { runtime().dispatcher.stop(); }

bool publish_big_app_started(pid_t pid, uint32_t app_id,
                             std::string_view title_id) {
  return publish(onion::lifecycle::EventType::BigAppStarted, pid, app_id,
                 title_id);
}

bool publish_big_app_exited(pid_t pid, uint32_t app_id,
                            std::string_view title_id) {
  return publish(onion::lifecycle::EventType::BigAppExited, pid, app_id,
                 title_id);
}

} // namespace onion::daemon::app_lifecycle
