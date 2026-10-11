// Linux integration test: production HTTPS transport and collection installer.
// sceNetInit is stubbed; use the cloud CA bundle for this environment's proxy.
#include "cheats/sync/http_transport_ps5.hpp"
#include "onion_cjson.hpp"
#include "patches/core.hpp"
#include <onion/log.h>
#include <cassert>
#include <filesystem>
#include <iostream>
extern "C" {
volatile int onion_log_runtime_level = ONION_LOG_INFO;
void onion_log_write(onion_log_level, const char *, ...) {}
int sceNetInit() { return 0; }
}
using namespace onion::cheats::sync;
using namespace hen331::patches;
int main() {
  Ps5HttpTransport http;
  auto get = [&](const std::string &url, const char *host, size_t max,
                 std::string &body) {
    HttpRequest req;
    req.url = url.c_str();
    req.host_allow = host;
    req.timeout_ms = 60000;
    req.max_body_bytes = max;
    req.follow_redirects = false;
    req.user_agent = "331HEN-GamePatches-verification";
    body.clear();
    return http.perform(req, [&](const void *p, size_t n) {
      body.append(static_cast<const char *>(p), n);
      return SyncStatus::Ok;
    });
  };
  const std::string api =
      "https://api.github.com/repos/illusionyy/PS-Game-Patch/commits/main";
  std::string data;
  assert(get(api, "unrelated.invalid", 1024 * 1024, data) ==
         SyncStatus::Rejected);
  assert(get(api, "api.github.com", 1, data) == SyncStatus::Protocol);
  assert(get(api, "api.github.com", 1024 * 1024, data) == SyncStatus::Ok);
  onion_cjson::Root j(data);
  std::string sha = onion_cjson::string_item(j.get(), "sha", "");
  assert(sha.size() == 40 &&
         sha.find_first_not_of("0123456789abcdef") == std::string::npos);
  assert(get("https://codeload.github.com/illusionyy/PS-Game-Patch/zip/" + sha,
             "codeload.github.com", 32 * 1024 * 1024, data) == SyncStatus::Ok);
  const std::string root = "/tmp/331hen-live-download-test";
  std::filesystem::remove_all(root);
  for (const auto &dir : {"xml", "config", "cache"})
    std::filesystem::create_directories(root + "/" + dir);
  assert(atomic_write(root + "/config/selection", "1\n"));
  assert(atomic_write(root + "/cache/download.zip", data));
  size_t count = 0;
  std::string error;
  assert(install_zip(root + "/cache/download.zip", root, count, error));
  assert(count > 0);
  std::cout << "PASS: production HTTPS transport (Linux libcurl, cloud CA, "
               "sceNetInit stub); host rejection, response size limit, live "
               "GitHub discovery, immutable download and validated install\n"
            << "revision=" << sha << " archive_bytes=" << data.size()
            << " definitions=" << count << " archive_sha256=" << digest(data)
            << "\n";
}
