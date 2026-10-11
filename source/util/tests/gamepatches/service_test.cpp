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
#include <sys/time.h>
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
uint64_t util_process_generation(pid_t requested) { return running && requested == pid ? generation : 0; }
int util_verify_game_identity(pid_t requested, int appid, uint64_t start, const char *title) {
 return running && requested == pid && appid == 10 && start == generation && std::string(title) == "CUSA00900" ? 0 : -1;
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
void metadata(const std::string &ver) {
 std::map<std::string,std::string> fields{{"TITLE_ID","CUSA00900"},{"TITLE","Bloodborne"},{"APP_VER",ver},{"VERSION","99.99"}};
 std::string keys, data, out(20 + fields.size()*16, '\0');
 auto u16=[&](size_t at,uint16_t v){out[at]=v;out[at+1]=v>>8;};
 auto u32=[&](size_t at,uint32_t v){for(unsigned i=0;i<4;i++)out[at+i]=v>>(8*i);};
 out.replace(0,4,"\0PSF",4);size_t i=0;
 for(auto &[k,v]:fields){size_t at=20+i++*16;u16(at,keys.size());u16(at+2,0x204);u32(at+4,v.size()+1);u32(at+8,v.size()+1);u32(at+12,data.size());keys+=k+'\0';data+=v+'\0';}
 u32(8,out.size());u32(12,out.size()+keys.size());u32(16,fields.size());out+=keys+data;
 std::filesystem::create_directories("/tmp/hen331-service-test/system_data/priv/appmeta/CUSA00900");
 std::ofstream f("/tmp/hen331-service-test/system_data/priv/appmeta/CUSA00900/param.sfo",std::ios::binary);f.write(out.data(),out.size());
}
void new_process() {
 ++pid;timeval now{};gettimeofday(&now,nullptr);
 generation=(uint64_t(now.tv_sec+2)<<32)|uint64_t(now.tv_usec);ram.clear();running=true;
}
int main(int argc, char **argv) {
 assert(argc==2);zip=file(argv[1]);
 std::filesystem::remove_all("/tmp/hen331-service-test");
 std::filesystem::create_directories("/tmp/hen331-service-test/user/app/CUSA00900");
 std::ofstream("/tmp/hen331-service-test/user/app/CUSA00900/app.pkg")<<"installed";
 metadata("01.09");initialize();initialize();
 assert(!file("/tmp/hen331-service-test/patches/xml/Bloodborne-Orbis.xml").empty());
 running=false;assert(field(games(0),"status").find("Choose")!=std::string::npos);
 std::string key,token=field(list(0,"CUSA00900"),"session");assert(!token.empty());
 for(int offset=0;offset<12;offset+=2){onion_cjson::Root j(list(offset,"CUSA00900"));cJSON *e; cJSON_ArrayForEach(e,cJSON_GetObjectItem(j.get(),"patches"))
 if(std::string(onion_cjson::string_item(e,"name",""))=="60 FPS (With Deltatime)")key=onion_cjson::string_item(e,"key","");}
 assert(!key.empty());assert(field(toggle(key,token,true),"status").starts_with("Saved ON"));assert(writes==0);
 assert(field(toggle(key,token,true),"status").starts_with("Saved ON"));assert(writes==0);
 assert(file("/tmp/hen331-service-test/patches/config/CUSA00900-"+key)=="1\n");
 running=true;assert(field(launch(pid,10,generation,"CUSA00900",version),"status").find("predates")!=std::string::npos);assert(writes==0);
 new_process();assert(field(launch(pid,10,generation,"CUSA00900",version),"status").find("scheduled")!=std::string::npos);tick();assert(writes>0);
 int n=writes;launch(pid,10,generation,"CUSA00900",version);tick();assert(writes==n);
 assert(field(toggle(key,token,false),"status").starts_with("Saved OFF"));assert(writes==n); // No live restoration of startup state.
 game_context_t game{};game.pid=pid;game.appid=10;strcpy(game.process_name,"eboot.bin");
 onion_patch_t patch{};patch.offset=0x00fbc40f-0x400000;patch.on_len=patch.off_len=2;patch.on[0]=0xab;
 onion_cheat_entry_t entry{};strcpy(entry.name,"Regression cheat");strcpy(entry.module_name,"eboot.bin");entry.patch_count=1;entry.patches=&patch;
 onion_cheat_file_t cheats{};cheats.master_code_id=-1;cheats.cheat_count=1;cheats.cheats=&entry;
 onion::cheats::CheatApplier applier;std::string status;
 assert(applier.toggle(game,cheats,0,status)<0 && status.find("Conflict")!=std::string::npos);
 ended(pid,generation);assert(hen331::memorycoord::patches().empty());
 new_process();game.pid=pid;assert(applier.toggle(game,cheats,0,status)==0);
 assert(field(toggle(key,token,true),"status").starts_with("Saved ON"));launch(pid,10,generation,"CUSA00900",version);n=writes;tick();assert(writes==n);
 assert(applier.toggle(game,cheats,0,status)==0);ended(pid,generation);new_process();
 external=true;launch(pid,10,generation,"CUSA00900",version);n=writes;tick();assert(writes==n);external=false;ended(pid,generation);
 new_process();version="01.08";launch(pid,10,generation,"CUSA00900",version);n=writes;tick();assert(writes==n);ended(pid,generation);version="01.09";
 metadata("01.08");assert(field(toggle(key,token,true),"status").find("changed")!=std::string::npos);
 assert(field(list(0,"CUSA00900"),"status")=="No compatible patches found.");metadata("01.09");
 token=field(list(0,"CUSA00900"),"session");assert(field(toggle(key,token,true),"status").starts_with("Saved ON"));
 download();assert(field(download(),"status")=="Download already running");wait_download();
 assert(field(list(0,"CUSA00900"),"collection").find("Installed/updated")!=std::string::npos);
 std::string active=active_dir("/tmp/hen331-service-test/patches");assert(!active.empty());network_failure=true;download();wait_download();
 assert(field(list(0,"CUSA00900"),"collection").find("previous collection preserved")!=std::string::npos);assert(active_dir("/tmp/hen331-service-test/patches")==active);
 network_failure=false;download();wait_download();assert(active_dir("/tmp/hen331-service-test/patches")==active);
 assert(file("/tmp/hen331-service-test/patches/config/CUSA00900-"+key)=="1\n");
 std::cout<<"PASS: service offline selection, persistence, verified launches, inherited/duplicate/mismatched processes, cheat overlap, failure/retry and settings preservation\n";
}
