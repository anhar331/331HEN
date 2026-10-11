#pragma once
#include <cstdint>
#include <map>
#include <mutex>
#include <vector>
namespace hen331::memorycoord {
struct Range {
  int pid;
  uint64_t start, size;
};
inline std::recursive_mutex &mutex() {
  static std::recursive_mutex m;
  return m;
}
inline std::map<const void *, std::vector<Range>> &cheats() {
  static std::map<const void *, std::vector<Range>> r;
  return r;
}
inline std::vector<Range> &patches() {
  static std::vector<Range> r;
  return r;
}
inline bool overlap(uint64_t a, uint64_t n, uint64_t b, uint64_t m) {
  return n && m && (a <= b ? b - a < n : a - b < m);
}
inline bool cheat_conflict(int pid, uint64_t address, uint64_t size) {
  for (const auto &[owner, list] : cheats())
    for (const auto &r : list)
      if (pid == r.pid && overlap(address, size, r.start, r.size))
        return true;
  return false;
}
inline bool patch_conflict(int pid, uint64_t address, uint64_t size) {
  for (const auto &r : patches())
    if (pid == r.pid && overlap(address, size, r.start, r.size))
      return true;
  return false;
}
} // namespace hen331::memorycoord
