#include "patches/service.hpp"
#include "cheats/i_memory_backend.hpp"
#include "cheats/sync/http_transport_ps5.hpp"
#include "onion_cjson.hpp"
#include "patches/coordination.hpp"
#include "patches/core.hpp"
#if !defined(ONION_HOST_TEST)
#include "pt.h"
#else
extern "C" int pt_attach_proc(int);
extern "C" int pt_detach_proc(int, int);
#endif
#include "util_platform.h"
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <mutex>
#include <onion/log.h>
#include <onion/notify.h>
#include <onion/proc_query.h>

#include <set>
#include <signal.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
extern "C" int sceShellCoreUtilGetAppLaunchedParamString(uint32_t, const char *,
                                                         char *, size_t);
extern "C" {
__asm__(
    ".pushsection .rodata\n.global "
    "gp_bloodborne_start\ngp_bloodborne_start:\n.incbin \"" GP_BLOODBORNE_XML
    "\"\n.global gp_bloodborne_end\ngp_bloodborne_end:\n.popsection\n");
extern const unsigned char gp_bloodborne_start[], gp_bloodborne_end[];
}
namespace hen331::patches {
namespace {
#ifndef GP_DATA_ROOT
#define GP_DATA_ROOT "/data/331HEN/patches"
#endif
const std::string root = GP_DATA_ROOT;
std::mutex mu;
Engine engine;
Target current;
std::vector<Patch> catalog;
std::map<std::string, std::string> states;
std::string catalog_error;
std::string update_status = "Collection not downloaded";
bool downloading = false, initialized = false;
int pending_pid = 0, attempts = 0;
int inherited_pid = 0;
uint64_t inherited_generation = 0;
bool inherited_session_blocked = false;
std::string pending_title;
std::string read_file(const std::string &p) {
  std::ifstream f(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)), {});
}
std::string json_status(const std::string &s) {
  cJSON *j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "status", s.c_str());
  return onion_cjson::print_owned(j);
}
void log(const std::string &s) {
  LOG_INFO("[patches] %s", s.c_str());
  FILE *f = fopen((root + "/logs/runtime.log").c_str(), "a");
  if (f) {
    fprintf(f, "%s\n", s.c_str());
    fclose(f);
  }
}
bool dirs() {
  for (const auto &p :
       {root.substr(0, root.find_last_of('/')), root, root + "/xml",
        root + "/config", root + "/cache", root + "/logs"}) {
    struct stat st{};
    if (lstat(p.c_str(), &st) == 0) {
      if (!S_ISDIR(st.st_mode))
        return false;
    } else if (mkdir(p.c_str(), 0700))
      return false;
  }
  return true;
}
bool external_engine() {
  return onion_find_pid("patch-server.elf") > 0 ||
         onion_find_pid("patch-server") > 0;
}
std::string session(const Target &t) {
  return std::to_string(t.pid) + ":" + std::to_string(t.appid) + ":" +
         std::to_string(t.generation) + ":" + t.title + ":" + t.version;
}
bool target(Target &t) {
  game_context_t game{};
  onion_bigapp_process_t process{};
  if (util_get_running_bigapp(&game) != 0 ||
      onion_resolve_running_bigapp(&process) != 0 || process.pid != game.pid ||
      process.session_generation == 0 || !title_id(game.title_id))
    return false;
  char live_title[16] = {};
  if (sceShellCoreUtilGetAppLaunchedParamString(
          static_cast<uint32_t>(game.appid), "TITLE_ID", live_title,
          sizeof(live_title)) < 0 ||
      strcmp(live_title, game.title_id) != 0)
    return false;
  t.pid = game.pid;
  t.appid = game.appid;
  t.generation = process.session_generation;
  t.title = game.title_id;
  t.elf = "eboot.bin";
  // Query launched update metadata; never guess from a filename or user input.
  char version[32] = {};
  if (sceShellCoreUtilGetAppLaunchedParamString(static_cast<uint32_t>(t.appid),
                                                "APP_VER", version,
                                                sizeof(version)) < 0 ||
      !version[0]) {
    if (sceShellCoreUtilGetAppLaunchedParamString(
            static_cast<uint32_t>(t.appid), "CONTENT_VERSION", version,
            sizeof(version)) < 0 ||
        !version[0])
      return false;
  }
  t.version = version;
  util_module_info_t module{};
  if (util_find_module(t.pid, "eboot.bin", &module) < 0)
    return false;
  t.base = module.sections[0].vaddr;
  if (!t.base)
    return false;
  t.regions.clear();
  for (const auto &section : module.sections)
    if (section.vaddr && section.size)
      t.regions.push_back({section.vaddr, section.size});
  return !t.regions.empty();
}
class RuntimeMemory final : public Memory {
  Target t_;
  std::unique_ptr<onion::cheats::IMemoryBackend> backend_;

public:
  explicit RuntimeMemory(const Target &t)
      : t_(t), backend_(onion::cheats::MemoryBackendFactory::create(
                   util_system_fw_major())) {}
  bool same(const Target &t) override {
    onion_bigapp_process_t p{};
    return onion_resolve_running_bigapp(&p) == 0 && p.pid == t.pid &&
           p.appid == t.appid && p.session_generation == t.generation;
  }
  bool freeze(const Target &t) override {
    if (!same(t))
      return false;
    if (pt_attach_proc(t.pid) == 0)
      return true;
    // The shared attach helper can fail after PT_ATTACH succeeds. Release our
    // trace.
    (void)pt_detach_proc(t.pid, 0);
    return false;
  }
  bool resume(const Target &t) override {
    return pt_detach_proc(t.pid, 0) == 0;
  }
  bool read(uint64_t a, void *b, size_t n) override {
    return backend_->read(t_.pid, a, b, n) == 0;
  }
  bool write(uint64_t a, const void *b, size_t n) override {
    return backend_->write(t_.pid, a, b, n) == 0;
  }
};
bool enabled(const Patch &p) {
  return read_file(root + "/config/" + current.title + "-" + p.key) == "1\n";
}
void load(const Target &t) {
  catalog.clear();
  catalog_error.clear();
  std::string dir = active_dir(root);
  if (dir.empty())
    dir = root + "/xml";
  DIR *d = opendir(dir.c_str());
  if (!d) {
    catalog_error = "Patch XML directory unavailable";
    return;
  }
  std::set<std::string> seen;
  while (auto *e = readdir(d)) {
    std::string n = e->d_name;
    if (!n.ends_with(".xml"))
      continue;
    struct stat st{};
    std::string file = dir + "/" + n;
    if (lstat(file.c_str(), &st) || !S_ISREG(st.st_mode) ||
        st.st_size > 2 * 1024 * 1024)
      continue;
    std::vector<Patch> parsed;
    std::string error;
    if (!parse(read_file(file), parsed, error)) {
      catalog_error = n + ": " + error;
      log(n + ": " + error);
      continue;
    }
    for (auto &p : parsed)
      if (matches(p, t.title, t.version) && seen.insert(p.key).second)
        catalog.push_back(std::move(p));
  }
  closedir(d);
}
bool ensure() {
  std::lock_guard<std::recursive_mutex> memory_lock(
      hen331::memorycoord::mutex());
  Target t;
  inherited_session_blocked = false;
  if (!initialized)
    return false;
  if (!target(t))
    return false;
  // A util restart loses the in-memory originals. Never capture already
  // patched bytes as new originals in a game inherited from an earlier util.
  if (t.pid == inherited_pid && t.generation == inherited_generation) {
    inherited_session_blocked = true;
    return false;
  }
  if (session(t) != session(current)) {
    current = t;
    engine.reset(t);
    hen331::memorycoord::patches().clear();
    states.clear();
    load(t);
  }
  return true;
}
void notify_result(const std::string &s) {
  log(s); /* Notification uses the existing linked notification transport. */
  onion_notify(true, "%s", s.c_str());
}
using onion::cheats::sync::SyncStatus;
bool fetch(const std::string &url, const char *host, std::string &data,
           size_t max) {
  onion::cheats::sync::Ps5HttpTransport http;
  onion::cheats::sync::HttpRequest req;
  req.url = url.c_str();
  req.host_allow = host;
  req.user_agent = "331HEN-GamePatches";
  req.timeout_ms = 90000;
  req.max_body_bytes = max;
  req.follow_redirects = false;
  data.clear();
  auto st = http.perform(req, [&](const void *p, size_t n) {
    data.append(static_cast<const char *>(p), n);
    return SyncStatus::Ok;
  });
  return st == SyncStatus::Ok;
}
void updater_body() {
  std::string data, error, sha;
  size_t count = 0;
  bool ok = false;
  for (int retry = 0; retry < 3 && !ok; ++retry) {
    if (!fetch("https://api.github.com/repos/illusionyy/PS-Game-Patch/commits/"
               "main",
               "api.github.com", data, 1024 * 1024)) {
      error = "GitHub discovery failed (network/TLS/rate limit)";
      continue;
    }
    onion_cjson::Root j(data);
    sha = onion_cjson::string_item(j.get(), "sha", "");
    if (sha.size() != 40 ||
        sha.find_first_not_of("0123456789abcdef") != std::string::npos) {
      error = "Invalid upstream revision";
      continue;
    }
    if (!fetch("https://codeload.github.com/illusionyy/PS-Game-Patch/zip/" +
                   sha,
               "codeload.github.com", data, 32 * 1024 * 1024)) {
      error = "Patch archive download failed";
      continue;
    }
    {
      std::lock_guard<std::mutex> lock(mu);
      if (!atomic_write(root + "/cache/download.zip", data)) {
        error = "Cannot store download (permission/space)";
        continue;
      }
      ok = install_zip(root + "/cache/download.zip", root, count, error);
      if (ok) {
        update_status = "Installed/updated " + std::to_string(count) +
                        " patches; revision " + sha;
        load(current);
      }
      unlink((root + "/cache/download.zip").c_str());
    }
  }
  std::string result;
  {
    std::lock_guard<std::mutex> lock(mu);
    downloading = false;
    if (!ok)
      update_status = "Update failed; previous collection preserved: " + error;
    result = update_status;
  }
  notify_result(result);
}
void updater() noexcept {
  try {
    updater_body();
  } catch (...) {
    std::lock_guard<std::mutex> lock(mu);
    downloading = false;
    update_status =
        "Update failed: resource error; previous collection preserved";
  }
}
} // namespace
void initialize() {
  std::lock_guard<std::mutex> lock(mu);
  if (initialized)
    return;
  initialized = true;
  onion_bigapp_process_t inherited{};
  if (onion_resolve_running_bigapp(&inherited) == 0) {
    inherited_pid = inherited.pid;
    inherited_generation = inherited.session_generation;
  }
  if (!dirs()) {
    update_status = "Patch storage unavailable";
    return;
  }
  if (active_dir(root).empty() &&
      access((root + "/xml/Bloodborne-Orbis.xml").c_str(), F_OK) != 0) {
    std::string xml(reinterpret_cast<const char *>(gp_bloodborne_start),
                    gp_bloodborne_end - gp_bloodborne_start);
    std::vector<Patch> p;
    std::string error;
    if (parse(xml, p, error))
      atomic_write(root + "/xml/Bloodborne-Orbis.xml", xml);
  }
  update_status = "Bundled Bloodborne collection; download for all games";
}
std::string list(size_t offset) {
  std::lock_guard<std::mutex> lock(mu);
  cJSON *j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "collection", update_status.c_str());
  bool ready = ensure();
  std::string status =
      ready ? (external_engine()
                   ? "Conflicting patch-server detected; patching blocked"
               : catalog.empty()
                   ? (catalog_error.empty()
                          ? "No compatible patches found."
                          : "Patch collection error: " + catalog_error)
                   : "Selections persist; writes use exact game "
                     "version and executable")
      : inherited_session_blocked
          ? "Restart game required: patch service inherited this "
            "running process"
          : "No running game with verified version/module mappings";
  cJSON_AddStringToObject(j, "status", status.c_str());
  cJSON_AddStringToObject(j, "session", session(current).c_str());
  cJSON_AddStringToObject(j, "title", ready ? current.title.c_str() : "");
  cJSON_AddStringToObject(j, "version", ready ? current.version.c_str() : "");
  cJSON *items = cJSON_AddArrayToObject(j, "patches");
  if (ready) {
    size_t end = std::min(catalog.size(), offset + 2);
    for (size_t i = offset; i < end; ++i) {
      const auto &p = catalog[i];
      cJSON *e = cJSON_CreateObject();
      cJSON_AddStringToObject(e, "key", p.key.c_str());
      cJSON_AddStringToObject(e, "name", p.name.substr(0, 128).c_str());
      cJSON_AddStringToObject(e, "author", p.author.substr(0, 128).c_str());
      cJSON_AddStringToObject(e, "note", p.note.substr(0, 160).c_str());
      cJSON_AddBoolToObject(e, "enabled", enabled(p));
      cJSON_AddBoolToObject(e, "supported", p.unsupported.empty());
      std::string state =
          p.unsupported.empty()
              ? (engine.applied(p.key) ? "Applied (memory verified)"
                 : enabled(p)          ? "Selected; restart game required"
                                       : "Off")
              : p.unsupported;
      if (states.count(p.key))
        state = states[p.key];
      cJSON_AddStringToObject(e, "status", state.substr(0, 160).c_str());
      cJSON_AddItemToArray(items, e);
    }
    cJSON_AddNumberToObject(j, "next",
                            end < catalog.size() ? static_cast<int>(end) : -1);
  } else
    cJSON_AddNumberToObject(j, "next", -1);
  return onion_cjson::print_owned(j);
}
std::string toggle(const std::string &key, const std::string &requested,
                   bool on) {
  std::lock_guard<std::mutex> lock(mu);
  std::lock_guard<std::recursive_mutex> memory_lock(
      hen331::memorycoord::mutex());
  if (!dirs())
    return json_status("Patch storage unavailable");
  if (!ensure() || session(current) != requested)
    return json_status(inherited_session_blocked
                           ? "Restart game required: clean patch session needed"
                           : "Game changed or terminated; reopen Game Patches");
  if (external_engine())
    return json_status("Conflicting patch-server active; patch not applied");
  auto it = std::find_if(catalog.begin(), catalog.end(),
                         [&](const Patch &p) { return p.key == key; });
  if (it == catalog.end() || !it->unsupported.empty())
    return json_status("Unsupported patch");
  // Persist only after verified application. Revert memory if persistence
  // fails.
  for (const auto &line : it->lines)
    if ((on || engine.applied(key)) &&
        hen331::memorycoord::cheat_conflict(
            current.pid, current.base + line.address - it->imagebase,
            line.bytes.size()))
      return json_status("Conflict with enabled Cheat; disable it first");
  RuntimeMemory m(current);
  std::string status;
  bool previous = engine.applied(key);
  if (engine.toggle(*it, current, m, on, status)) {
    if (!atomic_write(root + "/config/" + current.title + "-" + key,
                      on ? "1\n" : "0\n")) {
      std::string rollback;
      engine.toggle(*it, current, m, previous, rollback);
      status = "Selection save failed; " + rollback;
    }
  }
  hen331::memorycoord::patches().clear();
  for (auto r : engine.ranges())
    hen331::memorycoord::patches().push_back({current.pid, r.start, r.size});
  states[key] = status;
  log(it->name + ": " + status);
  return json_status(status);
}
std::string download() {
  std::lock_guard<std::mutex> lock(mu);
  if (downloading)
    return json_status("Download already running");
  if (!dirs())
    return json_status("Patch storage unavailable");
  downloading = true;
  update_status = "Downloading verified upstream patch collection";
  try {
    std::thread(updater).detach();
  } catch (...) {
    downloading = false;
    update_status = "Cannot start download worker; retry later";
  }
  return json_status(update_status);
}
std::string launch(int pid, const std::string &title) {
  std::lock_guard<std::mutex> lock(mu);
  if (pid <= 1 || !title_id(title))
    return json_status("Invalid launch");
  pending_pid = pid;
  pending_title = title;
  attempts = 50;
  return json_status("Launch scheduled");
}
void tick() try {
  std::lock_guard<std::mutex> lock(mu);
  std::lock_guard<std::recursive_mutex> memory_lock(
      hen331::memorycoord::mutex());
  if (attempts <= 0)
    return;
  --attempts;
  if (!ensure() || current.pid != pending_pid || current.title != pending_title)
    return;
  attempts = 0;
  if (external_engine()) {
    log("External patch-server active; launch patching blocked");
    return;
  }
  for (const auto &p : catalog)
    if (enabled(p) && p.unsupported.empty()) {
      bool conflict = false;
      for (const auto &l : p.lines)
        if (hen331::memorycoord::cheat_conflict(
                current.pid, current.base + l.address - p.imagebase,
                l.bytes.size()))
          conflict = true;
      if (conflict) {
        states[p.key] = "Conflict with enabled Cheat";
        continue;
      }
      RuntimeMemory m(current);
      std::string status;
      engine.toggle(p, current, m, true, status);
      states[p.key] = status;
      log(p.name + ": " + status);
    }
  hen331::memorycoord::patches().clear();
  for (auto r : engine.ranges())
    hen331::memorycoord::patches().push_back({current.pid, r.start, r.size});
} catch (...) {
  attempts = 0;
  LOG_ERROR(
      "[patches] launch processing failed; no further automatic attempts");
}
} // namespace hen331::patches
