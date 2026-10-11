// 331HEN native game patches. GPL-3.0-or-later; upstream format:
// illusionyy/ps-patch-system.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace hen331::patches {
struct Line {
  uint64_t address = 0;
  std::vector<uint8_t> bytes;
};
struct Patch {
  std::string key, name, author, note, version, elf, unsupported;
  std::vector<std::string> titles;
  uint64_t imagebase = 0x400000;
  std::vector<Line> lines;
};
bool title_id(const std::string &s);
bool parse(const std::string &xml, std::vector<Patch> &out, std::string &error);
bool matches(const Patch &p, const std::string &title,
             const std::string &version);
std::string digest(const std::string &s);
struct Region {
  uint64_t start, size;
};
struct Target {
  int pid = 0, appid = 0;
  uint64_t generation = 0, base = 0;
  std::string title, version, elf;
  std::vector<Region> regions;
};
struct Memory {
  virtual ~Memory() = default;
  virtual bool same(const Target &) = 0;
  virtual bool freeze(const Target &) = 0;
  virtual bool resume(const Target &) = 0;
  virtual bool read(uint64_t, void *, size_t) = 0;
  virtual bool write(uint64_t, const void *, size_t) = 0;
};
// All-or-rollback writes while target is stopped. Conflicts detected before
// writes.
class Engine {
  struct Owned {
    uint64_t address;
    std::vector<uint8_t> original, patched;
  };
  Target target_;
  std::map<std::string, std::vector<Owned>> owned_;
  bool poisoned_ = false;

public:
  void reset(const Target &target);
  std::vector<Region> ranges() const;
  bool applied(const std::string &key) const;
  bool toggle(const Patch &, const Target &, Memory &, bool enable,
              std::string &status);
};
bool atomic_write(const std::string &path, const std::string &contents);
bool safe_archive_path(const std::string &path);
bool install_zip(const std::string &zip, const std::string &root, size_t &count,
                 std::string &error);
std::string active_dir(const std::string &root);
} // namespace hen331::patches
