// 331HEN installed-game metadata discovery. GPL-3.0-or-later.
#include "patches/games.hpp"
#include "cJSON.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <set>
#include <sys/stat.h>
#include <unistd.h>

namespace hen331::patches {
namespace {
constexpr size_t metadata_limit = 256 * 1024;
constexpr size_t game_limit = 4096;
const char *const app_roots[] = {"/user/app", "/mnt/ext0/user/app",
                                "/mnt/ext1/user/app", "/system_ex/app"};
const char *const meta_roots[] = {"/system_data/priv/appmeta",
                                 "/system_data/priv/appmeta/external",
                                 "/user/appmeta"};
bool valid_title(const std::string &s) {
  return s.size() == 9 &&
         (s.starts_with("CUSA") || s.starts_with("CUSB") ||
          s.starts_with("PPSA") || s.starts_with("PPSB")) &&
         std::all_of(s.begin() + 4, s.end(),
                     [](char c) { return c >= '0' && c <= '9'; });
}
bool valid_version(const std::string &s, bool ps5) {
  if (s.empty() || s.size() > 31)
    return false;
  unsigned dots = 0;
  bool digit = false;
  for (char c : s) {
    if (c >= '0' && c <= '9')
      digit = true;
    else if (c == '.' && digit) {
      ++dots;
      digit = false;
    } else
      return false;
  }
  return digit && dots == (ps5 ? 2u : 1u);
}
bool root_path(const std::string &supplied, std::string &root) {
  root = supplied;
  while (!root.empty() && root.back() == '/')
    root.pop_back();
  if (root.empty())
    return true;
  if (root.front() != '/' || root.find('\0') != std::string::npos)
    return false;
  size_t pos = 1;
  while (pos <= root.size()) {
    size_t end = root.find('/', pos);
    if (end == std::string::npos)
      end = root.size();
    std::string component = root.substr(pos, end - pos);
    if (component.empty() || component == "." || component == "..")
      return false;
    pos = end + 1;
  }
  return true;
}
// Check every ancestor, not merely the metadata leaf. Runtime paths are fixed
// and title IDs are validated; no filename supplied by the UI is opened.
bool safe_ancestors(const std::string &path) {
  if (path.empty() || path.front() != '/')
    return false;
  size_t end = path.find('/', 1);
  while (end != std::string::npos) {
    struct stat st{};
    if (lstat(path.substr(0, end).c_str(), &st) != 0 ||
        !S_ISDIR(st.st_mode))
      return false;
    end = path.find('/', end + 1);
  }
  return true;
}
bool safe_directory(const std::string &path) {
  struct stat st{};
  return safe_ancestors(path) && lstat(path.c_str(), &st) == 0 &&
         S_ISDIR(st.st_mode);
}
bool regular_file(const std::string &path) {
  struct stat st{};
  return safe_ancestors(path) && lstat(path.c_str(), &st) == 0 &&
         S_ISREG(st.st_mode) && st.st_size > 0;
}
bool read_metadata(const std::string &path, std::string &data) {
  if (!safe_ancestors(path))
    return false;
  int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW);
  if (fd < 0)
    return false;
  struct stat st{};
  bool ok = fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0 &&
            static_cast<uint64_t>(st.st_size) <= metadata_limit;
  if (ok) {
    data.resize(static_cast<size_t>(st.st_size));
    size_t done = 0;
    while (done < data.size()) {
      ssize_t n = read(fd, data.data() + done, data.size() - done);
      if (n < 0 && errno == EINTR)
        continue;
      if (n <= 0) {
        ok = false;
        break;
      }
      done += static_cast<size_t>(n);
    }
  }
  close(fd);
  if (!ok)
    data.clear();
  return ok;
}
uint16_t le16(const unsigned char *p) {
  return static_cast<uint16_t>(p[0] | static_cast<unsigned>(p[1]) << 8);
}
uint32_t le32(const unsigned char *p) {
  return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
         static_cast<uint32_t>(p[2]) << 16 |
         static_cast<uint32_t>(p[3]) << 24;
}
bool sfo_fields(const std::string &data,
                std::map<std::string, std::string> &fields) {
  if (data.size() < 20 || data.compare(0, 4, "\0PSF", 4) != 0)
    return false;
  const auto *p = reinterpret_cast<const unsigned char *>(data.data());
  size_t keys = le32(p + 8), values = le32(p + 12), count = le32(p + 16);
  if (count > 4096 || count > (data.size() - 20) / 16 ||
      keys < 20 + count * 16 || keys >= values || values >= data.size())
    return false;
  for (size_t i = 0; i < count; ++i) {
    const auto *entry = p + 20 + i * 16;
    size_t key = le16(entry), length = le32(entry + 4),
           maximum = le32(entry + 8), value = le32(entry + 12);
    if (key >= values - keys || value >= data.size() - values ||
        length > maximum || length > data.size() - values - value)
      return false;
    const char *name = data.data() + keys + key;
    const void *name_end = memchr(name, 0, values - keys - key);
    if (!name_end)
      return false;
    std::string field(name, static_cast<const char *>(name_end) - name);
    if (field.empty() || fields.count(field))
      return false;
    if (le16(entry + 2) != 0x0204 && le16(entry + 2) != 0x0004)
      continue;
    const char *text = data.data() + values + value;
    const void *text_end = memchr(text, 0, length);
    if (length == 0 || !text_end)
      return false;
    fields.emplace(std::move(field),
                   std::string(text, static_cast<const char *>(text_end) - text));
  }
  return true;
}
bool safe_name(const std::string &name) {
  if (name.empty() || name.size() > 512)
    return false;
  // XML attributes cannot represent control bytes. Validate UTF-8 before
  // passing metadata names into the native ShellUI XML builder.
  for (size_t i = 0; i < name.size();) {
    unsigned char c = static_cast<unsigned char>(name[i]);
    if (c < 0x20 || c == 0x7f)
      return false;
    if (c < 0x80) {
      ++i;
      continue;
    }
    unsigned length;
    uint32_t code;
    if (c >= 0xc2 && c <= 0xdf) {
      length = 2;
      code = c & 0x1f;
    } else if (c >= 0xe0 && c <= 0xef) {
      length = 3;
      code = c & 0x0f;
    } else if (c >= 0xf0 && c <= 0xf4) {
      length = 4;
      code = c & 0x07;
    } else
      return false;
    if (length > name.size() - i)
      return false;
    for (unsigned j = 1; j < length; ++j) {
      unsigned char next = static_cast<unsigned char>(name[i + j]);
      if ((next & 0xc0) != 0x80)
        return false;
      code = code << 6 | (next & 0x3f);
    }
    if ((length == 3 && code < 0x800) ||
        (length == 4 && code < 0x10000) || code > 0x10ffff ||
        (code >= 0xd800 && code <= 0xdfff))
      return false;
    i += length;
  }
  return true;
}
std::string string_field(const cJSON *j, const char *key) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(j, key);
  return cJSON_IsString(item) && item->valuestring ? item->valuestring : "";
}
bool json_fields(const std::string &data, InstalledGame &game) {
  // Bound parser recursion and reject decoded embedded NULs, whose length is
  // not exposed by cJSON's string API.
  unsigned depth = 0;
  bool string = false, escaped = false;
  for (size_t i = 0; i < data.size(); ++i) {
    char c = data[i];
    if (string) {
      if (escaped) {
        if (c == 'u' && data.compare(i + 1, 4, "0000") == 0)
          return false;
        escaped = false;
      } else if (c == '\\')
        escaped = true;
      else if (c == '"')
        string = false;
    } else if (c == '"')
      string = true;
    else if (c == '{' || c == '[') {
      if (++depth > 32)
        return false;
    } else if (c == '}' || c == ']') {
      if (depth == 0)
        return false;
      --depth;
    }
  }
  const char *end = nullptr;
  cJSON *j = cJSON_ParseWithLengthOpts(data.data(), data.size(), &end, 0);
  if (!j)
    return false;
  while (end < data.data() + data.size() &&
         (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
    ++end;
  bool ok = cJSON_IsObject(j) && end == data.data() + data.size();
  std::set<std::string> keys;
  for (const cJSON *item = j->child; ok && item; item = item->next)
    ok = item->string && keys.insert(item->string).second;
  if (ok) {
    game.title = string_field(j, "titleId");
    game.version = string_field(j, "contentVersion");
    game.name = string_field(j, "titleName");
    const cJSON *localized = cJSON_GetObjectItemCaseSensitive(j, "localizedParameters");
    if (game.name.empty() && cJSON_IsObject(localized)) {
      const std::string language = string_field(localized, "defaultLanguage");
      const cJSON *entry = cJSON_GetObjectItemCaseSensitive(
          localized, language.empty() ? "en-US" : language.c_str());
      game.name = string_field(entry, "titleName");
      if (game.name.empty())
        for (const cJSON *item = localized->child; item; item = item->next) {
          game.name = string_field(item, "titleName");
          if (!game.name.empty())
            break;
        }
    }
  }
  cJSON_Delete(j);
  return ok;
}
std::vector<std::string> installs(const std::string &root,
                                   const std::string &title) {
  std::vector<std::string> result;
  for (const char *base : app_roots) {
    std::string app = root + base + "/" + title;
    if (!safe_directory(app))
      continue;
    if (regular_file(app + "/app.pkg") || regular_file(app + "/eboot.bin") ||
        regular_file(app + "/image0/eboot.bin"))
      result.push_back(std::move(app));
  }
  return result;
}
bool resolve(const std::string &title, InstalledGame &out, std::string &error,
              const std::string &root) {
  out = {};
  error.clear();
  if (!valid_title(title)) {
    error = "Invalid game Title ID";
    return false;
  }
  const auto proofs = installs(root, title);
  if (proofs.empty()) {
    error = "Game is not installed in an accessible application location";
    return false;
  }
  out.title = title;
  out.name = title;
  out.platform = title.starts_with("PPS") ? "ps5" : "ps4";
  out.install_path = proofs.front();
  bool ps5 = out.platform == "ps5";
  std::vector<std::string> bases;
  for (const char *base : meta_roots)
    bases.push_back(root + base + "/" + title);
  for (const auto &proof : proofs) {
    bases.push_back(proof + "/sce_sys");
    bases.push_back(proof);
    bases.push_back(proof + "/image0/sce_sys");
  }
  std::set<std::string> versions;
  bool have_metadata = false;
  for (const auto &base : bases) {
    std::string path = base + (ps5 ? "/param.json" : "/param.sfo"), data;
    if (!read_metadata(path, data))
      continue;
    InstalledGame meta;
    bool parsed;
    if (ps5)
      parsed = json_fields(data, meta);
    else {
      std::map<std::string, std::string> fields;
      parsed = sfo_fields(data, fields);
      meta.title = fields["TITLE_ID"];
      meta.name = fields["TITLE"];
      meta.version = fields["APP_VER"];
    }
    if (!parsed || meta.title != title)
      continue;
    if (!have_metadata) {
      out.metadata_path = path;
      if (safe_name(meta.name))
        out.name = meta.name;
    }
    have_metadata = true;
    if (valid_version(meta.version, ps5))
      versions.insert(meta.version);
  }
  if (versions.size() == 1)
    out.version = *versions.begin();
  else if (versions.size() > 1)
    error = "Installed metadata versions disagree; patch selection blocked";
  else
    error = have_metadata ? "Installed game version unavailable"
                          : "Installed game metadata unavailable or invalid";
  return true;
}
} // namespace
bool installed_game(const std::string &title, InstalledGame &out,
                    std::string &error, const std::string &filesystem_root) {
  std::string root;
  if (!root_path(filesystem_root, root)) {
    out = {};
    error = "Invalid filesystem root";
    return false;
  }
  return resolve(title, out, error, root);
}
InstalledGameCatalog installed_games(const std::string &filesystem_root) {
  InstalledGameCatalog result;
  std::string root;
  if (!root_path(filesystem_root, root)) {
    result.warnings.push_back("Invalid filesystem root");
    return result;
  }
  std::set<std::string> titles;
  for (const char *base : app_roots) {
    std::string path = root + base;
    if (!safe_directory(path))
      continue;
    DIR *directory = opendir(path.c_str());
    if (!directory)
      continue;
    while (const dirent *entry = readdir(directory)) {
      std::string title = entry->d_name;
      if (valid_title(title))
        titles.insert(std::move(title));
      if (titles.size() >= game_limit) {
        result.warnings.push_back("Installed game discovery limit reached");
        break;
      }
    }
    closedir(directory);
  }
  for (const auto &title : titles) {
    InstalledGame game;
    std::string error;
    if (!resolve(title, game, error, root))
      continue;
    if (!error.empty())
      result.warnings.push_back(title + ": " + error);
    result.games.push_back(std::move(game));
  }
  std::sort(result.games.begin(), result.games.end(),
            [](const InstalledGame &a, const InstalledGame &b) {
              return a.name == b.name ? a.title < b.title : a.name < b.name;
            });
  return result;
}
} // namespace hen331::patches
