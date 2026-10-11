#include "cheats/cheat_applier.hpp"
#include "cheats/i_memory_backend.hpp"
#include "cheats/sync/http_transport_ps5.hpp"
#include "onion_cjson.hpp"
#include "patches/coordination.hpp"
#include "patches/core.hpp"
#include "patches/service.hpp"
#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <onion/log.h>
#include <onion/notify.h>
#include <onion/proc_query.h>
#include <thread>
using namespace hen331::patches;
static bool running = true, external = false, network_failure = false;
static int pid = 42;
static uint64_t generation = 1;
static std::string version = "01.09", zip;
static std::map<uint64_t, uint8_t> ram;
static int writes = 0;
extern "C" {
volatile int onion_log_runtime_level = 0;
void onion_log_write(onion_log_level, const char *, ...) {}
void onion_notify(int, const char *, ...) {}
void onion_notify_format(char *out, size_t n, int, const char *s, va_list) {
  snprintf(out, n, "%s", s);
}
uint32_t util_system_fw_major() { return 0x1270; }
int util_find_module(int, const char *name, util_module_info_t *m) {
  if (strcmp(name, "eboot.bin"))
    return -1;
  memset(m, 0, sizeof(*m));
  m->sections[0].vaddr = 0x10000000;
  m->sections[0].size = 0x10000000;
  return 0;
}
int util_find_module_in_app(int, const char *, pid_t *, util_module_info_t *) {
  return -1;
}
int pt_attach_proc(int) { return running ? 0 : -1; }
int pt_detach_proc(int, int) { return 0; }
int util_get_running_bigapp(game_context_t *g) {
  if (!running)
    return -1;
  memset(g, 0, sizeof(*g));
  g->pid = pid;
  g->appid = 10;
  strcpy(g->title_id, "CUSA00900");
  strcpy(g->process_name, "eboot.bin");
  return 0;
}
int onion_resolve_running_bigapp(onion_bigapp_process_t *p) {
  if (!running)
    return -1;
  memset(p, 0, sizeof(*p));
  p->pid = pid;
  p->appid = 10;
  p->session_generation = generation;
  strcpy(p->process_name, "eboot.bin");
  return 0;
}
pid_t onion_find_pid(const char *) { return external ? 99 : -1; }
int sceShellCoreUtilGetAppLaunchedParamString(uint32_t, const char *key,
                                              char *out, size_t n) {
  snprintf(out, n, "%s",
           strcmp(key, "TITLE_ID") == 0 ? "CUSA00900" : version.c_str());
  return 0;
}
}
namespace onion::cheats {
class MockMemory final : public IMemoryBackend {
public:
  int read(pid_t, uint64_t a, void *p, size_t n) override {
    for (size_t i = 0; i < n; ++i)
      static_cast<uint8_t *>(p)[i] = ram[a + i];
    return 0;
  }
  int write(pid_t, uint64_t a, const void *p, size_t n) override {
    ++writes;
    for (size_t i = 0; i < n; ++i)
      ram[a + i] = static_cast<const uint8_t *>(p)[i];
    return 0;
  }
  int mapCodeCave(pid_t, uint64_t, size_t) override { return -1; }
};
std::unique_ptr<IMemoryBackend> MemoryBackendFactory::create(uint32_t) {
  return std::make_unique<MockMemory>();
}
namespace sync {
SyncStatus Ps5HttpTransport::perform(
    const HttpRequest &req,
    const std::function<SyncStatus(const void *, size_t)> &sink) {
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  assert(std::string(req.url).starts_with("https://"));
  assert(req.max_body_bytes > 0 && req.timeout_ms > 0);
  assert(!req.follow_redirects);
  if (network_failure)
    return SyncStatus::Network;
  std::string body =
      std::string(req.url).find("api.github.com") != std::string::npos
          ? "{\"sha\":\"0123456789012345678901234567890123456789\"}"
          : zip;
  return sink(body.data(), body.size());
}
} // namespace sync
} // namespace onion::cheats
std::string file(const std::string &p) {
  std::ifstream f(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)), {});
}
std::string field(const std::string &json, const char *key) {
  onion_cjson::Root j(json);
  return onion_cjson::string_item(j.get(), key, "");
}
void wait_download() {
  for (int i = 0; i < 500; ++i) {
    if (field(list(0), "collection").find("Downloading") == std::string::npos)
      return;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  assert(false);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  zip = file(argv[1]);
  std::filesystem::remove_all("/tmp/hen331-service-test");
  std::filesystem::create_directories("/tmp/hen331-service-test");
  initialize();
  auto before =
      file("/tmp/hen331-service-test/patches/xml/Bloodborne-Orbis.xml");
  assert(!before.empty());
  initialize();
  assert(file("/tmp/hen331-service-test/patches/xml/Bloodborne-Orbis.xml") ==
         before);
  assert(field(list(0), "status").starts_with("Restart game required"));
  assert(field(toggle("unknown", "", true), "status")
             .starts_with("Restart game required"));
  assert(writes == 0);
  ++generation; // A newly launched game establishes clean original bytes.
  std::string key, token = field(list(0), "session");
  for (int offset = 0; offset < 12; offset += 2) {
    onion_cjson::Root j(list(offset));
    cJSON *e;
    cJSON_ArrayForEach(e, cJSON_GetObjectItem(j.get(), "patches")) {
      if (std::string(onion_cjson::string_item(e, "name", "")) ==
          "60 FPS (With Deltatime)")
        key = onion_cjson::string_item(e, "key", "");
    }
  }
  assert(!key.empty());
  assert(field(toggle(key, token, true), "status").starts_with("Applied"));
  auto n = writes;
  assert(field(toggle(key, token, true), "status").starts_with("Already"));
  assert(writes == n);
  assert(file("/tmp/hen331-service-test/patches/config/CUSA00900-" + key) ==
         "1\n");
  game_context_t game{};
  game.pid = pid;
  game.appid = 10;
  strcpy(game.process_name, "eboot.bin");
  onion_patch_t patch{};
  patch.offset = 0x00fbc40f - 0x400000;
  patch.on_len = patch.off_len = 2;
  patch.on[0] = 0xab;
  onion_cheat_entry_t entry{};
  strcpy(entry.name, "Regression cheat");
  strcpy(entry.module_name, "eboot.bin");
  entry.patch_count = 1;
  entry.patches = &patch;
  onion_cheat_file_t cheats{};
  cheats.master_code_id = -1;
  cheats.cheat_count = 1;
  cheats.cheats = &entry;
  onion::cheats::CheatApplier applier;
  std::string status;
  assert(applier.toggle(game, cheats, 0, status) < 0 &&
         status.find("Conflict") != std::string::npos);
  assert(field(toggle(key, token, false), "status").starts_with("Disabled"));
  assert(applier.toggle(game, cheats, 0, status) == 0);
  assert(
      hen331::memorycoord::cheat_conflict(pid, 0x10000000 + patch.offset, 2));
  assert(field(toggle(key, token, true), "status").find("Conflict") !=
         std::string::npos);
  assert(applier.toggle(game, cheats, 0, status) == 0);
  assert(field(toggle(key, token, true), "status").starts_with("Applied"));
  ++pid;
  ++generation;
  ram.clear();
  launch(pid, "CUSA00900");
  tick();
  assert(writes > n);
  assert(field(toggle(key, token, false), "status").find("changed") !=
         std::string::npos);
  token = field(list(0), "session");
  assert(field(toggle(key, token, true), "status").starts_with("Already"));
  launch(pid, "CUSA00900");
  n = writes;
  tick();
  assert(n == writes);
  external = true;
  assert(field(toggle(key, token, false), "status").find("Conflicting") !=
         std::string::npos);
  external = false;
  running = false;
  assert(field(list(0), "status").find("No running") != std::string::npos);
  assert(field(toggle(key, token, false), "status").find("changed") !=
         std::string::npos);
  running = true;
  version = "01.08";
  assert(field(list(0), "status") == "No compatible patches found.");
  version = "01.09";
  download();
  assert(field(download(), "status") == "Download already running");
  wait_download();
  assert(field(list(0), "collection").find("Installed/updated") !=
         std::string::npos);
  std::string active = active_dir("/tmp/hen331-service-test/patches");
  assert(!active.empty());
  network_failure = true;
  download();
  wait_download();
  std::cerr << "EXPECTED FAILURE STATUS: " << field(list(0), "collection")
            << "\n";
  assert(field(list(0), "collection").find("previous collection preserved") !=
         std::string::npos);
  assert(active_dir("/tmp/hen331-service-test/patches") == active);
  network_failure = false;
  download();
  wait_download();
  assert(active_dir("/tmp/hen331-service-test/patches") == active);
  assert(file("/tmp/hen331-service-test/patches/config/CUSA00900-" + key) ==
         "1\n");
  std::cout << "PASS: production service mocks — initialization, no game, "
               "inherited process rejection, version rejection, independent "
               "persistent toggles, relaunch "
               "reapply, duplicate launch, engine conflict, cheat overlap in "
               "both directions, normal cheat toggle regression, async native "
               "download, failure/retry and settings preservation\n";
}
