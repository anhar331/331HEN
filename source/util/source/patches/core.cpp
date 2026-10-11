#include "patches/core.hpp"
#include <pugixml.hpp>
extern "C" {
#include <sha256.h>
}
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <set>
#include <sstream>
#include <unistd.h>
namespace hen331::patches {
std::string digest(const std::string &s) {
  SHA256_CTX c;
  uint8_t b[32];
  sha256_init(&c);
  sha256_update(&c, reinterpret_cast<const uint8_t *>(s.data()), s.size());
  sha256_final(&c, b);
  static const char h[] = "0123456789abcdef";
  std::string out;
  for (uint8_t v : b) {
    out += h[v >> 4];
    out += h[v & 15];
  }
  return out;
}
bool title_id(const std::string &s) {
  if (s.size() != 9 || (s.substr(0, 4) != "CUSA" && s.substr(0, 4) != "PPSA" &&
                        s.substr(0, 4) != "CUSB" && s.substr(0, 4) != "PPSB"))
    return false;
  return std::all_of(s.begin() + 4, s.end(),
                     [](char c) { return c >= '0' && c <= '9'; });
}
static bool number(const std::string &s, uint64_t &n) {
  if (s.empty() || s[0] == '-' || s[0] == '+' || s[0] == ' ' || s.size() > 22)
    return false;
  const char *p = s.c_str();
  int base = 10;
  if (*p == '$' || *p == '#') {
    ++p;
    base = 16;
  } else if (s.starts_with("0x") || s.starts_with("0X")) {
    p += 2;
    base = 16;
  }
  if (!*p)
    return false;
  errno = 0;
  char *end = nullptr;
  n = strtoull(p, &end, base);
  return errno == 0 && end != p && *end == 0;
}
static bool hexbytes(const std::string &s, std::vector<uint8_t> &out) {
  std::string h;
  for (char c : s) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
      continue;
    h += c;
  }
  if (h.empty() || h.size() % 2 || h.size() > 131072)
    return false;
  auto val = [](char c) {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    return -1;
  };
  for (size_t i = 0; i < h.size(); i += 2) {
    int a = val(h[i]), b = val(h[i + 1]);
    if (a < 0 || b < 0)
      return false;
    out.push_back(static_cast<uint8_t>((a << 4) | b));
  }
  return true;
}
static bool textbytes(const std::string &s, bool utf16,
                      std::vector<uint8_t> &out) {
  std::vector<uint8_t> raw;
  for (size_t i = 0; i < s.size(); ++i) {
    uint8_t c = static_cast<uint8_t>(s[i]);
    if (c == '\\') {
      if (++i >= s.size())
        return false;
      c = static_cast<uint8_t>(s[i]);
      if (c == 'n')
        c = '\n';
      else if (c == 'r')
        c = '\r';
      else if (c == 't')
        c = '\t';
      else if (c == '0')
        c = 0;
      else if (c == 'x') {
        if (i + 2 >= s.size())
          return false;
        std::vector<uint8_t> one;
        if (!hexbytes(s.substr(i + 1, 2), one))
          return false;
        c = one[0];
        i += 2;
      } else if (c != '\\')
        return false;
    }
    raw.push_back(c);
  }
  if (!utf16) {
    out = std::move(raw);
    return !out.empty();
  }
  // Genuine UTF-8 to UTF-16LE, independent of wchar_t width.
  for (size_t i = 0; i < raw.size();) {
    uint32_t c = raw[i++];
    unsigned more = 0;
    if (c >= 0x80) {
      if (c >= 0xc2 && c <= 0xdf) {
        more = 1;
        c &= 31;
      } else if (c >= 0xe0 && c <= 0xef) {
        more = 2;
        c &= 15;
      } else if (c >= 0xf0 && c <= 0xf4) {
        more = 3;
        c &= 7;
      } else
        return false;
      unsigned n = more;
      while (n--) {
        if (i >= raw.size() || (raw[i] & 0xc0) != 0x80)
          return false;
        c = (c << 6) | (raw[i++] & 63);
      }
      if ((more == 1 && c < 128) || (more == 2 && c < 2048) ||
          (more == 3 && c < 65536) || c > 0x10ffff ||
          (c >= 0xd800 && c <= 0xdfff))
        return false;
    }
    auto add = [&](uint32_t v) {
      out.push_back(v & 255);
      out.push_back((v >> 8) & 255);
    };
    if (c < 65536)
      add(c);
    else {
      c -= 0x10000;
      add(0xd800 + (c >> 10));
      add(0xdc00 + (c & 1023));
    }
  }
  return !out.empty();
}
static bool encode(const std::string &type, const std::string &s,
                   std::vector<uint8_t> &b) {
  if (type == "bytes")
    return hexbytes(s, b);
  if (type == "utf8" || type == "utf16")
    return textbytes(s, type == "utf16", b);
  size_t size = type == "byte"      ? 1
                : type == "bytes16" ? 2
                : type == "bytes32" ? 4
                : type == "bytes64" ? 8
                                    : 0;
  uint64_t n = 0;
  if (size) {
    if (!number(s, n) || (size < 8 && n >= (1ULL << (size * 8))))
      return false;
  } else if (type == "float32" || type == "float64") {
    char *end = nullptr;
    errno = 0;
    double v = strtod(s.c_str(), &end);
    if (s.empty() || end != s.c_str() + s.size() || errno || !std::isfinite(v))
      return false;
    if (type == "float32") {
      float f = static_cast<float>(v);
      if (!std::isfinite(f))
        return false;
      uint32_t u;
      memcpy(&u, &f, 4);
      n = u;
      size = 4;
    } else {
      memcpy(&n, &v, 8);
      size = 8;
    }
  } else
    return false;
  for (size_t i = 0; i < size; ++i)
    b.push_back((n >> (i * 8)) & 255);
  return true;
}
bool parse(const std::string &xml, std::vector<Patch> &out,
           std::string &error) {
  out.clear();
  error.clear();
  if (xml.empty() || xml.size() > 2 * 1024 * 1024 ||
      xml.find("<!DOCTYPE") != std::string::npos ||
      xml.find("<!ENTITY") != std::string::npos) {
    error = "Invalid or oversized XML";
    return false;
  }
  pugi::xml_document doc;
  auto res = doc.load_buffer(xml.data(), xml.size());
  auto root = doc.child("Patch");
  if (!res || !root) {
    error = "Malformed patch XML";
    return false;
  }
  std::vector<std::string> titles;
  for (auto id : root.child("TitleID").children("ID")) {
    std::string s = id.text().get();
    if (!title_id(s)) {
      error = "Invalid Title ID";
      return false;
    }
    titles.push_back(s);
  }
  if (titles.empty()) {
    error = "Missing Title ID";
    return false;
  }
  std::set<std::string> keys;
  for (auto meta : root.children("Metadata")) {
    if (out.size() >= 256) {
      error = "Too many patches";
      return false;
    }
    Patch p;
    p.titles = titles;
    p.name = meta.attribute("Name").value();
    p.author = meta.attribute("Author").value();
    p.note = meta.attribute("Note").value();
    p.version = meta.attribute("AppVer").value();
    p.elf = meta.attribute("AppElf").value();
    if (p.name.empty() || p.version.empty() || p.elf.empty() ||
        p.name.size() > 512 || p.note.size() > 4096) {
      error = "Incomplete Metadata";
      return false;
    }
    if (p.elf != "eboot.bin")
      p.unsupported = "Unsupported target executable/module";
    if (meta.attribute("ImageBase") &&
        !number(meta.attribute("ImageBase").value(), p.imagebase)) {
      error = "Invalid ImageBase";
      return false;
    }
    std::ostringstream identity;
    identity << p.name << '\n'
             << p.version << '\n'
             << p.elf << '\n'
             << meta.attribute("PatchVer").value() << '\n'
             << p.imagebase;
    for (const auto &id : titles)
      identity << '\n' << id;
    size_t bytes = 0;
    for (auto line : meta.child("PatchList").children("Line")) {
      if (p.lines.size() >= 4096) {
        error = "Too many patch lines";
        return false;
      }
      std::string type = line.attribute("Type").value(),
                  address = line.attribute("Address").value(),
                  value = line.attribute("Value").value();
      identity << '\n' << type << ':' << address << ':' << value;
      if (type == "mask" || type == "mask_jump32") {
        p.unsupported = "Pattern/cave operations unsupported";
        continue;
      }
      if (type != "byte" && type != "bytes16" && type != "bytes32" &&
          type != "bytes64" && type != "bytes" && type != "float32" &&
          type != "float64" && type != "utf8" && type != "utf16") {
        p.unsupported = "Unsupported patch operation: " + type.substr(0, 64);
        continue;
      }
      Line l;
      if (!number(address, l.address) || l.address < p.imagebase ||
          !encode(type, value, l.bytes)) {
        error = "Invalid address or value";
        return false;
      }
      bytes += l.bytes.size();
      if (bytes > 1024 * 1024) {
        error = "Patch exceeds byte limit";
        return false;
      }
      p.lines.push_back(std::move(l));
    }
    if (p.lines.empty() && p.unsupported.empty()) {
      error = "Empty PatchList";
      return false;
    }
    p.key = digest(identity.str());
    if (!keys.insert(p.key).second) {
      error = "Duplicate patch definition";
      return false;
    }
    out.push_back(std::move(p));
  }
  if (out.empty()) {
    error = "Missing Metadata";
    return false;
  }
  return true;
}
bool matches(const Patch &p, const std::string &title,
             const std::string &version) {
  if (!title_id(title) || version.empty() || version == "unknown" ||
      std::find(p.titles.begin(), p.titles.end(), title) == p.titles.end())
    return false;
  std::istringstream stream(p.version);
  std::string v;
  while (std::getline(stream, v, ',')) {
    auto a = v.find_first_not_of(" \t\r\n"), b = v.find_last_not_of(" \t\r\n");
    if (a != std::string::npos && v.substr(a, b - a + 1) == version &&
        version != "mask")
      return true;
  }
  return false;
}
void Engine::reset(const Target &t) {
  target_ = t;
  owned_.clear();
  poisoned_ = false;
}
std::vector<Region> Engine::ranges() const {
  std::vector<Region> r;
  for (const auto &[key, owned] : owned_)
    for (const auto &v : owned)
      r.push_back({v.address, v.patched.size()});
  return r;
}
bool Engine::applied(const std::string &key) const {
  return owned_.count(key) != 0;
}
bool Engine::toggle(const Patch &p, const Target &t, Memory &m, bool enable,
                    std::string &status) {
  auto fail = [&](const char *s) {
    status = s;
    return false;
  };
  if (t.pid != target_.pid || t.generation != target_.generation ||
      t.appid != target_.appid || t.title != target_.title ||
      t.version != target_.version)
    return fail("Game session changed; reopen Game Patches");
  if (poisoned_)
    return fail("Previous write/rollback failed; restart game required");
  if (!matches(p, t.title, t.version) || !p.unsupported.empty() ||
      p.elf != t.elf)
    return fail("Unsupported game/version/executable or operation");
  if (!m.same(t))
    return fail("Game process terminated or changed");
  if (enable == applied(p.key)) {
    if (enable) {
      std::vector<uint8_t> verify;
      for (const auto &range : owned_.at(p.key)) {
        verify.resize(range.patched.size());
        if (!m.read(range.address, verify.data(), verify.size()) ||
            verify != range.patched)
          return fail(
              "Previously applied bytes changed; restart game required");
      }
    }
    status = enable ? "Already applied (memory verified)" : "Already disabled";
    return true;
  }
  std::vector<Owned> changes;
  if (enable) {
    for (const auto &line : p.lines) {
      uint64_t off = line.address - p.imagebase;
      if (t.base > std::numeric_limits<uint64_t>::max() - off)
        return fail("Address overflow");
      uint64_t addr = t.base + off;
      if (addr > std::numeric_limits<uint64_t>::max() - line.bytes.size())
        return fail("Address overflow");
      bool mapped = false;
      for (auto region : t.regions)
        if (addr >= region.start && addr - region.start <= region.size &&
            line.bytes.size() <= region.size - (addr - region.start))
          mapped = true;
      if (!mapped)
        return fail("Patch address outside verified module mappings");
      for (const auto &[key, owned] : owned_)
        for (const auto &o : owned)
          if (addr < o.address + o.patched.size() &&
              o.address < addr + line.bytes.size())
            return fail("Conflict with an enabled patch; disable it first");
      changes.push_back({addr, {}, line.bytes});
    }
    std::map<uint64_t, uint8_t> unique;
    for (const auto &c : changes)
      for (size_t i = 0; i < c.patched.size(); ++i) {
        auto [it, inserted] = unique.emplace(c.address + i, c.patched[i]);
        if (!inserted && it->second != c.patched[i])
          return fail("Conflicting lines inside patch");
      }
    changes.clear();
    for (auto it = unique.begin(); it != unique.end();) {
      Owned c;
      c.address = it->first;
      uint64_t next = it->first;
      while (it != unique.end() && it->first == next) {
        c.patched.push_back(it->second);
        ++it;
        ++next;
      }
      c.original.resize(c.patched.size());
      changes.push_back(std::move(c));
    }
  } else
    changes = owned_.at(p.key);
  // Allocate transaction bookkeeping before stopping the process.
  size_t maximum = 0;
  for (const auto &c : changes)
    maximum = std::max(maximum, c.patched.size());
  std::vector<uint8_t> verify(maximum);
  std::map<std::string, std::vector<Owned>> prepared;
  if (enable)
    prepared.emplace(p.key, std::vector<Owned>{});
  if (!m.freeze(t))
    return fail("Could not safely stop game; patch not applied");
  struct StoppedTransaction {
    Memory &memory;
    const Target &target;
    std::vector<Owned> &ranges;
    bool enable;
    bool &poisoned;
    size_t attempted = 0;
    bool active = true;
    ~StoppedTransaction() {
      if (!active)
        return;
      // Exception recovery: undo every attempted write, then release the trace.
      for (size_t i = attempted; i > 0; --i) {
        auto &c = ranges[i - 1];
        const auto &bytes = enable ? c.original : c.patched;
        if (!memory.same(target) ||
            !memory.write(c.address, bytes.data(), bytes.size()))
          poisoned = true;
      }
      if (!memory.resume(target))
        poisoned = true;
    }
  } transaction{m, t, changes, enable, poisoned_};
  bool ok = m.same(t);
  const char *why = "Memory read failed; patch not applied";
  for (auto &c : changes) {
    if (!ok)
      break;
    if (enable)
      ok = m.read(c.address, c.original.data(), c.original.size());
    else {
      ok = m.read(c.address, verify.data(), c.patched.size());
      if (ok &&
          !std::equal(c.patched.begin(), c.patched.end(), verify.begin())) {
        ok = false;
        why = "Memory changed by another component; restart game required";
      }
    }
  }
  if (ok)
    for (auto &c : changes) {
      const auto &bytes = enable ? c.patched : c.original;
      ++transaction.attempted;
      if (!m.same(t) || !m.write(c.address, bytes.data(), bytes.size()) ||
          !m.read(c.address, verify.data(), bytes.size()) ||
          !std::equal(bytes.begin(), bytes.end(), verify.begin())) {
        ok = false;
        why = "Memory write/readback failed";
        break;
      }
    }
  bool rollback = true;
  if (!ok) {
    for (size_t i = transaction.attempted; i > 0; --i) {
      auto &c = changes[i - 1];
      const auto &bytes = enable ? c.original : c.patched;
      if (!m.same(t) || !m.write(c.address, bytes.data(), bytes.size()) ||
          !m.read(c.address, verify.data(), bytes.size()) ||
          !std::equal(bytes.begin(), bytes.end(), verify.begin()))
        rollback = false;
    }
    if (!rollback)
      poisoned_ = true;
  }
  transaction.active = false;
  bool resumed = m.resume(t);
  if (ok) {
    if (enable) {
      prepared.at(p.key) = std::move(changes);
      owned_.merge(prepared);
    } else
      owned_.erase(p.key);
  }
  if (!resumed) {
    poisoned_ = true;
    return fail("Game resume failed; restart game required");
  }
  if (!ok) {
    status = rollback ? std::string(why) + "; no changes retained"
                      : "Rollback failed; restart game required";
    return false;
  }
  status = enable ? "Applied (memory readback verified)"
                  : "Disabled (original bytes restored)";
  return true;
}
bool atomic_write(const std::string &path, const std::string &contents) {
  std::string tmp = path + ".tmp";
  int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600);
  if (fd < 0)
    return false;
  size_t n = 0;
  bool ok = true;
  while (n < contents.size()) {
    ssize_t w = write(fd, contents.data() + n, contents.size() - n);
    if (w <= 0) {
      ok = false;
      break;
    }
    n += static_cast<size_t>(w);
  }
  if (fsync(fd))
    ok = false;
  if (close(fd))
    ok = false;
  if (ok && rename(tmp.c_str(), path.c_str()) == 0)
    return true;
  unlink(tmp.c_str());
  return false;
}
bool safe_archive_path(const std::string &p) {
  if (p.empty() || p.size() > 768 || p[0] == '/' ||
      p.find('\\') != std::string::npos || p.find(':') != std::string::npos)
    return false;
  std::istringstream s(p);
  std::string part;
  while (std::getline(s, part, '/'))
    if (part.empty() || part == "." || part == "..")
      return false;
  return true;
}
} // namespace hen331::patches
