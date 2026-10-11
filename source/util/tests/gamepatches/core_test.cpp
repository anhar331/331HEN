#include "patches/core.hpp"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace hen331::patches;
struct Fake final : Memory {
  std::map<uint64_t, uint8_t> ram;
  bool alive = true, stopped = false, resume_ok = true, stop_ok = true;
  int reads = 0, writes = 0, fail_write = 0, fail_read = 0;
  bool same(const Target &) override { return alive; }
  bool freeze(const Target &) override {
    if (!stop_ok)
      return false;
    stopped = true;
    return true;
  }
  bool resume(const Target &) override {
    stopped = false;
    return resume_ok;
  }
  bool read(uint64_t a, void *p, size_t n) override {
    if (++reads == fail_read)
      return false;
    for (size_t i = 0; i < n; ++i)
      static_cast<uint8_t *>(p)[i] = ram[a + i];
    return alive;
  }
  bool write(uint64_t a, const void *p, size_t n) override {
    assert(stopped);
    if (++writes == fail_write)
      return false;
    for (size_t i = 0; i < n; ++i)
      ram[a + i] = static_cast<const uint8_t *>(p)[i];
    return alive;
  }
};
std::string file(const char *p) {
  std::ifstream f(p);
  return std::string((std::istreambuf_iterator<char>(f)), {});
}
int main(int argc, char **argv) {
  assert(argc >= 3);
  std::vector<Patch> p;
  std::string error;
  assert(parse(file(argv[1]), p, error));
  size_t compatible = 0;
  Patch fps;
  for (auto &v : p)
    if (matches(v, "CUSA00900", "01.09")) {
      ++compatible;
      assert(!matches(v, "CUSA00900", "01.08"));
      assert(!matches(v, "CUSA00000", "01.09"));
      if (v.name == "60 FPS (With Deltatime)")
        fps = v;
    }
  assert(compatible >= 4 && !fps.lines.empty() && fps.unsupported.empty());
  for (const std::string &name :
       {"Skip Intro", "Disable Motion Blur", "Disable Chromatic Aberration"}) {
    bool found = false;
    for (auto &v : p)
      if (v.name == name && matches(v, "CUSA00900", "01.09")) {
        found = true;
        assert(v.unsupported.empty());
      }
    assert(found);
  }
  Target t;
  t.pid = 42;
  t.appid = 10;
  t.generation = 100;
  t.title = "CUSA00900";
  t.version = "01.09";
  t.elf = "eboot.bin";
  t.base = 0x10000000;
  t.regions = {{t.base, 0x10000000}};
  Engine e;
  e.reset(t);
  Fake m;
  std::string status;
  auto before = m.ram;
  assert(e.toggle(fps, t, m, true, status));
  assert(e.applied(fps.key));
  int writes = m.writes;
  assert(e.toggle(fps, t, m, true, status));
  assert(m.writes == writes);
  assert(e.toggle(fps, t, m, false, status));
  assert(!e.applied(fps.key));
  for (const auto &[a, v] : m.ram)
    assert(v == 0);
  assert(!m.stopped);
  e.reset(t);
  m = Fake{};
  m.fail_write = 2;
  assert(!e.toggle(fps, t, m, true, status));
  for (const auto &[a, v] : m.ram)
    assert(v == 0);
  assert(!e.applied(fps.key) && !m.stopped);
  m = Fake{};
  m.fail_read = 2;
  assert(!e.toggle(fps, t, m, true, status));
  assert(m.writes == 0 && !m.stopped);
  m = Fake{};
  m.alive = false;
  assert(!e.toggle(fps, t, m, true, status));
  assert(m.writes == 0);
  m = Fake{};
  m.stop_ok = false;
  assert(!e.toggle(fps, t, m, true, status));
  assert(m.writes == 0);
  m = Fake{};
  auto wrong = t;
  wrong.version = "01.08";
  assert(!e.toggle(fps, wrong, m, true, status));
  assert(m.writes == 0);
  wrong = t;
  wrong.generation++;
  assert(!e.toggle(fps, wrong, m, true, status));
  assert(m.writes == 0);
  e.reset(t);
  m = Fake{};
  assert(e.toggle(fps, t, m, true, status));
  Patch conflict = fps;
  conflict.key = "conflict";
  assert(!e.toggle(conflict, t, m, true, status));
  auto unmapped = t;
  unmapped.regions = {{t.base, 1}};
  e.reset(unmapped);
  m = Fake{};
  assert(!e.toggle(fps, unmapped, m, true, status));
  assert(m.writes == 0);
  e.reset(t);
  m = Fake{};
  assert(e.toggle(fps, t, m, true, status));
  m.ram[t.base + fps.lines[0].address - fps.imagebase] ^= 1;
  assert(!e.toggle(fps, t, m, false, status));
  assert(!m.stopped);
  assert(!parse("<Patch>", p, error));
  assert(!parse("<!DOCTYPE Patch><Patch/>", p, error));
  assert(
      !parse("<Patch><TitleID><ID>../../bad</ID></TitleID></Patch>", p, error));
  for (const std::string &bad :
       {"../escape.xml", "foo/../../escape.xml", "/absolute.xml", "a\\b",
        "a/./x", "a//b", "C:/bad"})
    assert(!safe_archive_path(bad));
  assert(safe_archive_path("repo/patches/xml/Bloodborne-Orbis.xml"));
  std::filesystem::path root = "/tmp/hen331-patch-test";
  std::filesystem::remove_all(root);
  for (const auto &dir : {"xml", "config", "cache"})
    std::filesystem::create_directories(root / dir);
  assert(atomic_write((root / "config/setting").string(), "1\n"));
  assert(file((root / "config/setting").c_str()) == "1\n");
  assert(atomic_write((root / "config/setting").string(), "0\n"));
  size_t count = 0;
  if (!install_zip(argv[2], root.string(), count, error)) {
    std::cerr << "INSTALL ERROR: " << error << "\n";
    return 2;
  }
  assert(count > 100);
  auto active = active_dir(root.string());
  assert(!active.empty());
  assert(install_zip(argv[2], root.string(), count, error));
  assert(active == active_dir(root.string()));
  assert(file((root / "config/setting").c_str()) == "0\n");
  assert(!install_zip("/nonexistent", root.string(), count, error));
  assert(active == active_dir(root.string()));
  std::ofstream corrupt(root / "cache/bad.zip");
  corrupt << "bad";
  corrupt.close();
  assert(!install_zip((root / "cache/bad.zip").string(), root.string(), count,
                      error));
  assert(active == active_dir(root.string()));
  for (int i = 3; i < argc; ++i) {
    assert(!install_zip(argv[i], root.string(), count, error));
    assert(active == active_dir(root.string()));
    assert(file((root / "config/setting").c_str()) == "0\n");
  }
  auto snapshot = fps;
  snapshot.imagebase = 0;
  snapshot.lines[0].address = UINT64_MAX;
  e.reset(t);
  m = Fake{};
  assert(!e.toggle(snapshot, t, m, true, status));
  assert(m.writes == 0);
  e.reset(t);
  m = Fake{};
  m.resume_ok = false;
  assert(!e.toggle(fps, t, m, true, status));
  assert(status.find("resume failed") != std::string::npos);
  std::vector<Patch> unsupported;
  assert(parse(
      "<Patch><TitleID><ID>CUSA00900</ID></TitleID><Metadata Name='Pattern' "
      "AppVer='01.09' AppElf='eboot.bin'><PatchList><Line Type='mask' "
      "Address='AA ?? BB' Value='90'/></PatchList></Metadata></Patch>",
      unsupported, error));
  assert(!unsupported[0].unsupported.empty());
  assert(!matches(fps, "CUSA00900", "mask"));
  assert(!atomic_write((root / "missing/setting").string(), "1"));
  std::cout
      << "PASS: parser, strict title/version, real Bloodborne " << compatible
      << " matching definitions, independent activation, restore, duplicate, "
         "conflicts, mapped bounds, failures, termination, session identity, "
         "atomic persistence and real archive update/retry/failure recovery\n";
}
