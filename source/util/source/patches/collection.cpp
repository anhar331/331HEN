#include "patches/core.hpp"
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <miniz.h>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
namespace hen331::patches {
static void remove_stage(const std::string &dir) {
  DIR *d = opendir(dir.c_str());
  if (!d)
    return;
  while (auto *e = readdir(d)) {
    std::string n = e->d_name;
    if (n == "." || n == "..")
      continue;
    struct stat st{};
    std::string p = dir + "/" + n;
    if (lstat(p.c_str(), &st) == 0) {
      if (S_ISDIR(st.st_mode))
        remove_stage(p);
      else
        unlink(p.c_str());
    }
  }
  closedir(d);
  rmdir(dir.c_str());
}
std::string active_dir(const std::string &root) {
  std::ifstream f(root + "/config/current");
  std::string id;
  std::getline(f, id);
  if (id.size() != 64 ||
      id.find_first_not_of("0123456789abcdef") != std::string::npos)
    return {};
  return root + "/xml/" + id;
}
bool install_zip(const std::string &zip, const std::string &root, size_t &count,
                 std::string &error) {
  count = 0;
  error.clear();
  struct stat st{};
  if (lstat(zip.c_str(), &st) || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
      st.st_size > 32 * 1024 * 1024) {
    error = "Invalid archive size";
    return false;
  }
  std::ifstream f(zip, std::ios::binary);
  std::string archive((std::istreambuf_iterator<char>(f)), {});
  std::string generation = digest(archive);
  char pattern[1024];
  snprintf(pattern, sizeof(pattern), "%s/xml/.stage-XXXXXX", root.c_str());
  if (!mkdtemp(pattern)) {
    error = "Cannot create staging directory";
    return false;
  }
  std::string stage = pattern;
  mz_zip_archive z{};
  if (!mz_zip_reader_init_file(&z, zip.c_str(), 0)) {
    remove_stage(stage);
    error = "Invalid ZIP";
    return false;
  }
  if (mz_zip_reader_get_num_files(&z) > 10000) {
    mz_zip_reader_end(&z);
    remove_stage(stage);
    error = "Too many archive entries";
    return false;
  }
  bool has_xml = false;
  for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&z); ++i) {
    mz_zip_archive_file_stat e{};
    if (mz_zip_reader_file_stat(&z, i, &e) &&
        std::string(e.m_filename).find("patches/xml/") != std::string::npos)
      has_xml = true;
  }
  bool ok = true;
  size_t total = 0;
  std::set<std::string> names;
  for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&z) && ok; ++i) {
    mz_zip_archive_file_stat entry{};
    if (!mz_zip_reader_file_stat(&z, i, &entry)) {
      ok = false;
      break;
    }
    std::string name = entry.m_filename;
    if (!safe_archive_path(name)) {
      error = "Archive path traversal rejected";
      ok = false;
      break;
    }
    if (((entry.m_external_attr >> 16) & S_IFMT) == S_IFLNK) {
      error = "Archive symlink rejected";
      ok = false;
      break;
    }
    if (mz_zip_reader_is_file_a_directory(&z, i))
      continue;
    const std::string marker = "/patches/";
    size_t at = name.find(marker);
    std::string relative =
        at == std::string::npos
            ? (name.starts_with("patches/") ? name.substr(8) : name)
            : name.substr(at + marker.size());
    if (has_xml && relative.starts_with("xml_orbis/"))
      continue;
    std::string platform = relative.starts_with("xml/")            ? "orbis"
                           : relative.starts_with("xml_orbis/")    ? "orbis"
                           : relative.starts_with("xml_prospero/") ? "prospero"
                                                                   : "";
    if (platform.empty() || !relative.ends_with(".xml"))
      continue;
    std::string base = relative.substr(relative.find('/') + 1);
    if (base.find('/') != std::string::npos || base.size() > 160 ||
        base.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTU"
                               "VWXYZ0123456789_-.") != std::string::npos) {
      error = "Unsafe XML filename";
      ok = false;
      break;
    }
    std::string dest = platform + "-" + base;
    if (!names.insert(dest).second || entry.m_uncomp_size > 2 * 1024 * 1024 ||
        entry.m_uncomp_size == 0 ||
        total + entry.m_uncomp_size > 64 * 1024 * 1024 || names.size() > 4096) {
      error = "Duplicate/oversized XML collection";
      ok = false;
      break;
    }
    total += entry.m_uncomp_size;
    size_t size = 0;
    void *data = mz_zip_reader_extract_to_heap(&z, i, &size, 0);
    if (!data) {
      error = "ZIP decompression/CRC failed";
      ok = false;
      break;
    }
    std::string xml(static_cast<char *>(data), size);
    free(data);
    std::vector<Patch> patches;
    if (!parse(xml, patches, error) || !atomic_write(stage + "/" + dest, xml)) {
      if (error.empty())
        error = "Storage write failed";
      ok = false;
      break;
    }
    count += patches.size();
  }
  mz_zip_reader_end(&z);
  if (!ok || count == 0) {
    if (error.empty())
      error = "No valid patch definitions";
    remove_stage(stage);
    count = 0;
    return false;
  }
  std::string installed = root + "/xml/" + generation;
  if (rename(stage.c_str(), installed.c_str()) != 0) {
    struct stat existing{};
    if (lstat(installed.c_str(), &existing) || !S_ISDIR(existing.st_mode)) {
      error = "Cannot install snapshot";
      remove_stage(stage);
      return false;
    }
    remove_stage(stage);
  }
  std::string previous = active_dir(root);
  if (!atomic_write(root + "/config/current", generation + "\n")) {
    error = "Cannot activate snapshot; old collection preserved";
    return false;
  }
  // Retain the active snapshot and one previous snapshot, never settings.
  DIR *dir = opendir((root + "/xml").c_str());
  if (dir) {
    while (auto *e = readdir(dir)) {
      std::string id = e->d_name, path = root + "/xml/" + id;
      if (id.size() == 64 &&
          id.find_first_not_of("0123456789abcdef") == std::string::npos &&
          path != installed && path != previous)
        remove_stage(path);
    }
    closedir(dir);
  }
  return true;
}
} // namespace hen331::patches
