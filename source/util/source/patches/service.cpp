#include "patches/service.hpp"
#include "cheats/i_memory_backend.hpp"
#include "cheats/sync/http_transport_ps5.hpp"
#include "onion_cjson.hpp"
#include "patches/coordination.hpp"
#include "patches/core.hpp"
#include "patches/games.hpp"
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
#include <sstream>
#include <sys/stat.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
extern "C" int sceShellCoreUtilGetAppLaunchedParamString(uint32_t, const char *, char *, size_t);
extern "C" {
__asm__(".pushsection .rodata\n.global gp_bloodborne_start\ngp_bloodborne_start:\n.incbin \"" GP_BLOODBORNE_XML "\"\n.global gp_bloodborne_end\ngp_bloodborne_end:\n.popsection\n");
extern const unsigned char gp_bloodborne_start[], gp_bloodborne_end[];
__asm__(".pushsection .rodata\n.global gp_yotei_start\ngp_yotei_start:\n.incbin \"" GP_YOTEI_XML "\"\n.global gp_yotei_end\ngp_yotei_end:\n.popsection\n");
extern const unsigned char gp_yotei_start[], gp_yotei_end[];
}
namespace hen331::patches {
namespace {
#ifndef GP_DATA_ROOT
#define GP_DATA_ROOT "/data/331HEN/patches"
#endif
#ifndef GP_GAME_ROOT
#define GP_GAME_ROOT ""
#endif
const std::string root = GP_DATA_ROOT;
std::mutex mu;
Engine engine;
Target current, pending;
InstalledGame selected;
std::vector<Patch> selected_catalog, runtime_catalog;
std::set<std::pair<std::string, std::string>> available_versions;
std::string availability_dir, availability_error;
bool availability_valid = false;
std::map<std::string, std::string> states;
std::string selected_error, runtime_status = "Launch a selected game to apply saved patches";
std::string update_status = "Collection not downloaded";
bool downloading = false, initialized = false;
int attempts = 0;
uint64_t initialized_generation = 0;
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
  if (f) { fprintf(f, "%s\n", s.c_str()); fclose(f); }
}
bool dirs() {
  for (const auto &p : {root.substr(0, root.find_last_of('/')), root, root + "/xml", root + "/custom", root + "/config", root + "/cache", root + "/logs"}) {
    struct stat st{};
    if (lstat(p.c_str(), &st) == 0) { if (!S_ISDIR(st.st_mode)) return false; }
    else if (mkdir(p.c_str(), 0700)) return false;
  }
  return true;
}
bool external_engine() {
  return onion_find_pid("patch-server.elf") > 0 || onion_find_pid("patch-server") > 0;
}
std::string session(const Target &t) {
  return std::to_string(t.pid) + ":" + std::to_string(t.appid) + ":" + std::to_string(t.generation) + ":" + t.title + ":" + t.version;
}
std::string selection_token() {
  std::string identity = selected.title + "\n" + selected.version;
  for (const auto &p : selected_catalog) identity += "\n" + p.key;
  return selected.title + ":" + selected.version + ":" + digest(identity);
}
bool enabled(const Patch &p, const std::string &title) {
  return read_file(root + "/config/" + title + "-" + p.key) == "1\n";
}
std::vector<std::string> collection_dirs() {
  std::string dir = active_dir(root);
  if (dir.empty()) dir = root + "/xml";
  return {dir, root + "/custom"};
}
// Stream each file once. Local definitions survive downloaded collection changes.
void load(const std::string &title, const std::string &version, std::vector<Patch> &out, std::string &error,
          bool metadata_only = false) {
  out.clear(); error.clear(); std::set<std::string> seen;
  size_t files = 0;
  for (const auto &dir : collection_dirs()) {
    DIR *d = opendir(dir.c_str());
    if (!d) { if (dir != root + "/custom") error = "Patch XML directory unavailable"; continue; }
    while (auto *e = readdir(d)) {
      std::string n = e->d_name;
      if (!n.ends_with(".xml")) continue;
      if (++files > 4096) { error = "Patch collection exceeds indexing limit"; break; }
      struct stat st{}; std::string file = dir + "/" + n;
      if (lstat(file.c_str(), &st) || !S_ISREG(st.st_mode) || st.st_size > 2 * 1024 * 1024) continue;
      std::vector<Patch> parsed; std::string parse_error;
      if (!parse(read_file(file), parsed, parse_error)) { error = n + ": " + parse_error; continue; }
      for (auto &patch : parsed) {
        if ((metadata_only || matches(patch, title, version)) && seen.insert(patch.key).second) {
          if (metadata_only) { patch.lines.clear(); patch.lines.shrink_to_fit(); patch.note.clear(); }
          out.push_back(std::move(patch));
        }
      }
      if (out.size() > 65536) { error = "Patch metadata exceeds indexing limit"; break; }
    }
    closedir(d);
    if (files > 4096 || out.size() > 65536) { out.clear(); return; }
  }
}
// Compact metadata index: parse once per explicit refresh, not per installed game.
void refresh_availability(bool force) {
  auto dirs = collection_dirs(); std::string dir = dirs.front();
  if (!force && availability_valid && availability_dir == dir) return;
  available_versions.clear(); availability_error.clear(); availability_dir = dir;
  availability_valid = true;
  std::vector<Patch> metadata;
  load("", "", metadata, availability_error, true);
  for (const auto &patch : metadata) {
    if (!patch.unsupported.empty()) continue;
    std::istringstream versions(patch.version); std::string version;
    while (std::getline(versions, version, ',')) {
      auto begin = version.find_first_not_of(" \t\r\n"), end = version.find_last_not_of(" \t\r\n");
      if (begin == std::string::npos) continue;
      std::string exact = version.substr(begin, end - begin + 1);
      for (const auto &title : patch.titles) {
        if (matches(patch, title, exact)) available_versions.emplace(title, exact);
      }
    }
  }
}
bool select(const std::string &title, bool refresh = false) {
  InstalledGame actual; std::string error;
  if (!installed_game(title, actual, error, GP_GAME_ROOT)) {
    selected = {}; selected_catalog.clear(); selected_error = error; return false;
  }
  if (actual.version.empty() || actual.version == "unknown") {
    selected = actual; selected_catalog.clear(); selected_error = "Installed game version unavailable; patch matching blocked"; return false;
  }
  if (refresh || selected.title != actual.title || selected.version != actual.version || selected.metadata_path != actual.metadata_path) {
    selected = std::move(actual);
    load(selected.title, selected.version, selected_catalog, selected_error);
  }
  return true;
}
bool target(Target &t, std::string &error) {
  t = pending;
  if (util_verify_game_identity(t.pid, t.appid, t.generation, t.title.c_str()) != 0) {
    error = "Game terminated or launch identity changed"; return false;
  }
  InstalledGame installed; std::string metadata_error;
  if (!installed_game(t.title, installed, metadata_error, GP_GAME_ROOT) || installed.version.empty()) {
    error = "Verified installed game/version unavailable: " + metadata_error; return false;
  }
  // APP_VER / CONTENT_VERSION are launched metadata. SystemService BigApp APIs
  // are deliberately not called from util: its credentials remain PTRACE.
  char live_title[16]{};
  int title_result = sceShellCoreUtilGetAppLaunchedParamString(static_cast<uint32_t>(t.appid), "TITLE_ID", live_title, sizeof(live_title));
  if (title_result >= 0 && live_title[0] && t.title != live_title) {
    error = "Launched Title ID mismatch; patching blocked"; return false;
  }
  char version[32]{};
  int version_result = sceShellCoreUtilGetAppLaunchedParamString(static_cast<uint32_t>(t.appid), "APP_VER", version, sizeof(version));
  if (version_result < 0 || !version[0]) {
    version[0] = 0;
    version_result = sceShellCoreUtilGetAppLaunchedParamString(static_cast<uint32_t>(t.appid), "CONTENT_VERSION", version, sizeof(version));
  }
  if (version_result >= 0 && version[0]) {
    if (!t.version.empty() && t.version != version) {
      error = "Launched version changed; patching blocked"; return false;
    }
    t.version = version;
  }
  // A new launch resolves its installed metadata only after the independently
  // verified PID, generation, application ID and Title ID establish identity.
  if (t.version.empty()) t.version = installed.version;
  if (t.version != installed.version) {
    error = "Installed/launched version mismatch; patching blocked"; return false;
  }
  t.elf = "eboot.bin";
  util_module_info_t module{};
  if (util_find_module(t.pid, "eboot.bin", &module) < 0 || !module.sections[0].vaddr) {
    error = "Executable module mappings unavailable; no patches applied"; return false;
  }
  t.base = module.sections[0].vaddr; t.regions.clear();
  for (const auto &section : module.sections)
    if (section.vaddr && section.size) t.regions.push_back({section.vaddr, section.size});
  if (t.regions.empty()) {
    error = "Executable mapping sizes unavailable; no guessed bounds used"; return false;
  }
  return true;
}
class RuntimeMemory final : public Memory {
  Target t_;
  std::unique_ptr<onion::cheats::IMemoryBackend> backend_;
public:
  explicit RuntimeMemory(const Target &t) : t_(t), backend_(onion::cheats::MemoryBackendFactory::create(util_system_fw_major())) {}
  bool same(const Target &t) override {
    return util_verify_game_identity(t.pid, t.appid, t.generation, t.title.c_str()) == 0;
  }
  bool freeze(const Target &t) override {
    if (!same(t)) return false;
    if (pt_attach_proc(t.pid) == 0) return true;
    return false;
  }
  bool resume(const Target &t) override { return pt_detach_proc(t.pid, 0) == 0; }
  bool read(uint64_t a, void *b, size_t n) override { return backend_ && backend_->read(t_.pid, a, b, n) == 0; }
  bool write(uint64_t a, const void *b, size_t n) override { return backend_ && backend_->write(t_.pid, a, b, n) == 0; }
};
void notify_result(const std::string &s) { log(s); onion_notify(true, "%s", s.c_str()); }
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
        selected = {};
        selected_catalog.clear();
        availability_valid = false;
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
  if (initialized) return;
  // Process generation uses ki_start timeval. Do not inherit a process that
  // predates this service: its original bytes may belong to an older util.
  timeval now{};
  if (gettimeofday(&now, nullptr) == 0)
    initialized_generation = (static_cast<uint64_t>(now.tv_sec) << 32) | static_cast<uint64_t>(now.tv_usec);
  initialized = true;
  if (!dirs()) { update_status = "Patch storage unavailable"; return; }
  if (active_dir(root).empty() && access((root + "/xml/Bloodborne-Orbis.xml").c_str(), F_OK) != 0) {
    std::string xml(reinterpret_cast<const char *>(gp_bloodborne_start), gp_bloodborne_end - gp_bloodborne_start);
    std::vector<Patch> patches; std::string error;
    if (parse(xml, patches, error)) atomic_write(root + "/xml/Bloodborne-Orbis.xml", xml);
  }
  const std::string local_yotei = root + "/custom/GhostYotei-PPSA26344-TEST.xml";
  if (access(local_yotei.c_str(), F_OK) != 0) {
    std::string xml(reinterpret_cast<const char *>(gp_yotei_start), gp_yotei_end - gp_yotei_start);
    std::vector<Patch> patches; std::string error;
    if (parse(xml, patches, error)) atomic_write(local_yotei, xml);
  }
  update_status = "Bundled Bloodborne and experimental Ghost Yotei definitions; download to update the collection";
}
std::string games(size_t offset, bool refresh) {
  std::lock_guard<std::mutex> lock(mu);
  auto catalog = installed_games(GP_GAME_ROOT);
  refresh_availability(refresh && offset == 0);
  std::erase_if(catalog.games, [](const InstalledGame &game) {
    return !available_versions.contains({game.title, game.version});
  });
  if (!availability_error.empty()) catalog.warnings.insert(catalog.warnings.begin(), availability_error);
  cJSON *j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "collection", update_status.c_str());
  cJSON_AddStringToObject(j, "status", catalog.games.empty() ? "No installed games with supported patches for their exact versions" : "Choose a game with compatible patches and save selections before launching it");
  if (!catalog.warnings.empty()) cJSON_AddStringToObject(j, "warning", catalog.warnings.front().substr(0, 180).c_str());
  cJSON *items = cJSON_AddArrayToObject(j, "games");
  size_t end = std::min(catalog.games.size(), offset + 4);
  for (size_t i = offset; i < end; ++i) {
    const auto &game = catalog.games[i]; cJSON *e = cJSON_CreateObject();
    cJSON_AddStringToObject(e, "title", game.title.c_str());
    cJSON_AddStringToObject(e, "name", game.name.substr(0, 160).c_str());
    cJSON_AddStringToObject(e, "version", game.version.c_str());
    cJSON_AddStringToObject(e, "platform", game.platform.c_str());
    cJSON_AddItemToArray(items, e);
  }
  cJSON_AddNumberToObject(j, "next", end < catalog.games.size() ? static_cast<int>(end) : -1);
  return onion_cjson::print_owned(j);
}
std::string list(size_t offset, const std::string &title, bool refresh) {
  std::lock_guard<std::mutex> lock(mu);
  std::string requested = title.empty() ? selected.title : title;
  bool ready = initialized && select(requested, refresh && offset == 0);
  cJSON *j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "collection", update_status.c_str());
  std::string status = !ready ? selected_error : selected_catalog.empty() ? "No compatible patches found." : "Save selections, then launch or restart the game. Startup effects are not guaranteed.";
  if (ready && (current.title == selected.title || pending.title == selected.title)) status = runtime_status;
  cJSON_AddStringToObject(j, "status", status.c_str());
  cJSON_AddStringToObject(j, "session", ready ? selection_token().c_str() : "");
  cJSON_AddStringToObject(j, "title", selected.title.c_str());
  cJSON_AddStringToObject(j, "name", selected.name.substr(0, 160).c_str());
  cJSON_AddStringToObject(j, "version", selected.version.c_str());
  cJSON *items = cJSON_AddArrayToObject(j, "patches");
  size_t end = ready ? std::min(selected_catalog.size(), offset + 2) : offset;
  if (ready) for (size_t i = offset; i < end; ++i) {
    const auto &p = selected_catalog[i]; cJSON *e = cJSON_CreateObject();
    bool on = enabled(p, selected.title);
    cJSON_AddStringToObject(e, "key", p.key.c_str());
    cJSON_AddStringToObject(e, "name", p.name.substr(0, 128).c_str());
    cJSON_AddStringToObject(e, "author", p.author.substr(0, 128).c_str());
    cJSON_AddStringToObject(e, "note", p.note.substr(0, 160).c_str());
    cJSON_AddBoolToObject(e, "enabled", on);
    cJSON_AddBoolToObject(e, "supported", p.unsupported.empty());
    bool live = current.title == selected.title && current.version == selected.version;
    std::string state = !p.unsupported.empty() ? p.unsupported : on ? "Selected; launch/restart game required" : "Off";
    if (live && engine.applied(p.key)) state = on ? "Written after launch (memory verified); startup effects unverified" : "Disable saved; restart game required";
    else if (live && states.count(p.key) && on) state = states[p.key];
    cJSON_AddStringToObject(e, "status", state.substr(0, 160).c_str());
    cJSON_AddItemToArray(items, e);
  }
  cJSON_AddNumberToObject(j, "next", ready && end < selected_catalog.size() ? static_cast<int>(end) : -1);
  return onion_cjson::print_owned(j);
}
std::string toggle(const std::string &key, const std::string &requested, bool on) {
  std::lock_guard<std::mutex> lock(mu);
  if (!initialized || !dirs()) return json_status("Patch storage unavailable");
  // Re-read installed metadata before accepting a stale page's selection.
  if (selected.title.empty() || !select(selected.title) || selection_token() != requested)
    return json_status("Installed game/version or collection changed; reopen Game Patches");
  auto it = std::find_if(selected_catalog.begin(), selected_catalog.end(), [&](const Patch &p) { return p.key == key; });
  if (it == selected_catalog.end() || !it->unsupported.empty()) return json_status("Unsupported patch");
  if (on) {
    // Detect address conflicts among independently selected patches before launch.
    for (const auto &other : selected_catalog) {
      if (other.key == key || !enabled(other, selected.title)) continue;
      for (const auto &a : it->lines) for (const auto &b : other.lines) {
        uint64_t aa = a.address - it->imagebase, bb = b.address - other.imagebase;
        if (aa < bb + b.bytes.size() && bb < aa + a.bytes.size())
          return json_status("Conflict with another selected patch; disable it first");
      }
    }
  }
  if (!atomic_write(root + "/config/" + selected.title + "-" + key, on ? "1\n" : "0\n"))
    return json_status("Selection save failed (permission or storage); previous setting retained");
  std::string status = on ? "Saved ON; launch/restart game required" : "Saved OFF; restart game required if already applied";
  log(selected.title + " / " + selected.version + " / " + it->name + ": " + status);
  return json_status(status);
}
std::string download() {
  std::lock_guard<std::mutex> lock(mu);
  if (downloading) return json_status("Download already running");
  if (!dirs()) return json_status("Patch storage unavailable");
  downloading = true; update_status = "Downloading verified upstream patch collection";
  try { std::thread(updater).detach(); }
  catch (...) { downloading = false; update_status = "Cannot start download worker; retry later"; }
  return json_status(update_status);
}
std::string launch(int pid, int appid, uint64_t generation, const std::string &title, const std::string &version) {
  std::lock_guard<std::mutex> lock(mu);
  if (!initialized || pid <= 1 || appid < 0 || !generation || !title_id(title) || version.size() > 31)
    return json_status("Invalid or unverified launch snapshot");
  if (util_verify_game_identity(pid, appid, generation, title.c_str()) != 0)
    return json_status("Launch identity changed; patching blocked");
  if (!initialized_generation || generation <= initialized_generation) {
    runtime_status = "Restart game required: process predates patch service";
    return json_status(runtime_status);
  }
  if (current.pid == pid && current.generation == generation) return json_status("Launch already processed; no duplicate writes");
  if (pending.pid == pid && pending.generation == generation && attempts > 0) return json_status("Launch already scheduled");
  pending = {}; pending.pid = pid; pending.appid = appid; pending.generation = generation; pending.title = title; pending.version = version;
  attempts = 50; runtime_status = "Verifying new launch metadata and executable mappings";
  return json_status("Launch scheduled; application occurs after process launch");
}
std::string ended(int pid, uint64_t generation) {
  std::lock_guard<std::mutex> lock(mu);
  std::lock_guard<std::recursive_mutex> memory_lock(hen331::memorycoord::mutex());
  if (pending.pid == pid && (!generation || pending.generation == generation)) { attempts = 0; pending = {}; }
  if (current.pid == pid && (!generation || current.generation == generation)) {
    current = {}; engine.reset(current); runtime_catalog.clear(); states.clear(); hen331::memorycoord::patches().clear();
    runtime_status = "Game closed; saved selections will apply on its next launch";
  }
  return json_status("Game exit recorded");
}
void tick() try {
  std::lock_guard<std::mutex> lock(mu);
  std::lock_guard<std::recursive_mutex> memory_lock(hen331::memorycoord::mutex());
  if (!initialized || attempts <= 0) return;
  --attempts;
  Target t; std::string error;
  if (!target(t, error)) {
    runtime_status = error;
    if (!attempts) log(pending.title + ": " + error + "; launch patches skipped");
    return;
  }
  attempts = 0;
  if (external_engine()) { runtime_status = "Conflicting patch-server active; launch patching blocked"; log(runtime_status); return; }
  if (session(current) != session(t)) {
    current = t; engine.reset(t); states.clear(); hen331::memorycoord::patches().clear();
    load(t.title, t.version, runtime_catalog, error);
  }
  size_t applied = 0;
  for (const auto &p : runtime_catalog) if (enabled(p, current.title) && p.unsupported.empty()) {
    bool conflict = false;
    for (const auto &line : p.lines)
      if (hen331::memorycoord::cheat_conflict(current.pid, current.base + line.address - p.imagebase, line.bytes.size())) conflict = true;
    if (conflict) { states[p.key] = "Conflict with enabled Cheat; not applied"; continue; }
    RuntimeMemory m(current); std::string status;
    if (engine.toggle(p, current, m, true, status)) {
      ++applied; status = "Written after launch (memory verified); startup effects unverified";
    }
    states[p.key] = status; log(current.title + ": " + p.name + ": " + status);
  }
  hen331::memorycoord::patches().clear();
  for (auto r : engine.ranges()) hen331::memorycoord::patches().push_back({current.pid, r.start, r.size});
  runtime_status = "Verified " + std::to_string(applied) + " saved patch writes after launch; pre-entry timing unverified";
} catch (...) {
  attempts = 0; LOG_ERROR("[patches] launch processing failed; no further automatic attempts");
}
} // namespace hen331::patches
