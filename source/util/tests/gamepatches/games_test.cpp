// Installed-game discovery uses fixture trees, never the user's console.
#include "patches/games.hpp"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <unistd.h>
using namespace hen331::patches;
namespace fs = std::filesystem;
namespace {
unsigned checks = 0;
void check(bool value, const char *message) {
  ++checks;
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    std::abort();
  }
}
void put16(std::string &s, size_t offset, unsigned value) {
  s[offset] = static_cast<char>(value);
  s[offset + 1] = static_cast<char>(value >> 8);
}
void put32(std::string &s, size_t offset, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i)
    s[offset + i] = static_cast<char>(value >> (8 * i));
}
std::string sfo(const std::map<std::string, std::string> &fields) {
  std::string keys, values;
  std::string result(20 + fields.size() * 16, '\0');
  result.replace(0, 4, "\0PSF", 4);
  put32(result, 4, 0x101);
  size_t i = 0;
  for (const auto &[key, value] : fields) {
    size_t index = 20 + i++ * 16;
    put16(result, index, keys.size());
    put16(result, index + 2, 0x204);
    put32(result, index + 4, value.size() + 1);
    put32(result, index + 8, value.size() + 1);
    put32(result, index + 12, values.size());
    keys += key + '\0';
    values += value + '\0';
  }
  put32(result, 8, result.size());
  put32(result, 12, result.size() + keys.size());
  put32(result, 16, fields.size());
  return result + keys + values;
}
void write(const fs::path &path, const std::string &data) {
  fs::create_directories(path.parent_path());
  std::ofstream f(path, std::ios::binary);
  f.write(data.data(), data.size());
  check(f.good(), "fixture written");
}
void install(const fs::path &root, const std::string &title,
             const std::string &base = "/user/app",
             const std::string &proof = "app.pkg") {
  write(root.string() + base + "/" + title + "/" + proof, "installed payload");
}
void ps4(const fs::path &root, const std::string &title,
         const std::string &version, const std::string &name = "Bloodborne",
         const std::string &base = "/system_data/priv/appmeta") {
  write(root.string() + base + "/" + title + "/param.sfo",
        sfo({{"TITLE_ID", title}, {"APP_VER", version}, {"TITLE", name},
             {"VERSION", "99.99"}}));
}
bool contains(const InstalledGameCatalog &catalog, const std::string &title) {
  return std::any_of(catalog.games.begin(), catalog.games.end(),
                     [&](const auto &game) { return game.title == title; });
}
} // namespace
int main() {
  const fs::path root = "/tmp/331hen-installed-games-test-" + std::to_string(getpid());
  fs::remove_all(root);
  fs::create_directories(root);
  InstalledGame game;
  std::string error;
  check(installed_games(root.string()).games.empty(), "empty system has no games");
  check(!installed_game("CUSA00900", game, error, root.string()),
        "absent game rejected");
  ps4(root, "CUSA00900", "01.09");
  check(!installed_game("CUSA00900", game, error, root.string()),
        "cached metadata does not prove installation");
  install(root, "CUSA00900");
  check(installed_game("CUSA00900", game, error, root.string()),
        "installed Bloodborne found");
  check(error.empty(), "valid metadata has no warning");
  check(game.title == "CUSA00900", "actual title identity preserved");
  check(game.version == "01.09", "APP_VER used instead of higher VERSION");
  check(game.name == "Bloodborne", "actual SFO title displayed");
  check(game.platform == "ps4", "PS4 platform identified");
  check(!game.install_path.empty() && !game.metadata_path.empty(),
        "metadata and install evidence paths returned");
  install(root, "CUSA00900", "/mnt/ext0/user/app");
  ps4(root, "CUSA00900", "01.09", "Bloodborne",
      "/system_data/priv/appmeta/external");
  check(installed_games(root.string()).games.size() == 1,
        "same title on internal/external deduplicated");
  fs::remove_all(root.string() + "/user/app/CUSA00900");
  check(installed_game("CUSA00900", game, error, root.string()),
        "external installed game supported");
  check(game.install_path.find("/mnt/ext0/") != std::string::npos,
        "external installation evidence retained");
  ps4(root, "CUSA00900", "01.08", "Bloodborne",
      "/system_data/priv/appmeta/external");
  check(installed_game("CUSA00900", game, error, root.string()) &&
            game.version.empty() && error.find("disagree") != std::string::npos,
        "inconsistent version evidence blocks compatibility");
  ps4(root, "CUSA00900", "01.09", "Bloodborne",
      "/system_data/priv/appmeta/external");
  fs::remove_all(root.string() + "/mnt/ext0/user/app/CUSA00900");
  check(!contains(installed_games(root.string()), "CUSA00900"),
        "deleted game excluded even when metadata remains");
  install(root, "CUSA00900");

  install(root, "PPSA12345");
  const std::string yotei_app = root.string() + "/data/homebrew/PPSA26344-app";
  const std::string yotei_meta = R"({"titleId":"PPSA26344","contentVersion":"01.008.000","originContentVersion":"01.000.000","targetContentVersion":"01.007.000","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"Ghost of Yōtei"}}})";
  write(yotei_app + "/sce_sys/param.json", yotei_meta);
  check(!contains(installed_games(root.string()), "PPSA26344"),
        "extracted metadata without an executable is not installation evidence");
  write(yotei_app + "/eboot.bin", "installed executable");
  check(installed_game("PPSA26344", game, error, root.string()) &&
            game.version == "01.008.000" && game.name == "Ghost of Yōtei" &&
            error.empty() && game.install_path == yotei_app,
        "homebrew title-app layout uses exact contentVersion and localized name");
  check(contains(installed_games(root.string()), "PPSA26344"),
        "homebrew title-app layout is enumerated in installed picker");
  fs::rename(yotei_app, root.string() + "/data/homebrew/PPSA26344-wrong");
  check(!contains(installed_games(root.string()), "PPSA26344"),
        "unrecognized directory suffix is not guessed as an installation");
  fs::rename(root.string() + "/data/homebrew/PPSA26344-wrong", yotei_app);
  fs::remove(yotei_app + "/eboot.bin");
  fs::create_symlink(root.string() + "/user/app/PPSA12345/app.pkg", yotei_app + "/eboot.bin");
  check(!installed_game("PPSA26344", game, error, root.string()),
        "homebrew executable symlinks cannot prove installation");
  fs::remove_all(yotei_app);
  write(root.string() + "/system_data/priv/appmeta/PPSA12345/param.json",
        R"({"titleId":"PPSA12345","contentVersion":"01.005.000","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"Native Game"}}})");
  check(installed_game("PPSA12345", game, error, root.string()) &&
            game.version == "01.005.000" && game.name == "Native Game",
        "PS5 exact contentVersion and localized title parsed");
  check(game.platform == "ps5", "PS5 platform identified");
  write(root.string() + "/system_data/priv/appmeta/PPSA12345/param.json",
        R"({"titleId":"PPSA12345","contentVersion":"01.006.000","version":"99.999.999","titleName":"Arabic لعبة"})");
  check(installed_game("PPSA12345", game, error, root.string()) &&
            game.version == "01.006.000" && game.name == "Arabic لعبة",
        "actual contentVersion and UTF-8 name preserved without normalization");
  install(root, "PPSB00002", "/user/app", "eboot.bin");
  write(root.string() + "/user/app/PPSB00002/sce_sys/param.json",
        R"({"titleId":"PPSB00002","contentVersion":"00.001.000","titleName":"Homebrew"})");
  check(installed_game("PPSB00002", game, error, root.string()) &&
            game.version == "00.001.000",
        "homebrew eboot installation and installed sce_sys metadata supported");
  install(root, "CUSB00003", "/mnt/ext1/user/app", "image0/eboot.bin");
  ps4(root, "CUSB00003", "01.00", "Image application", "/user/appmeta");
  check(installed_game("CUSB00003", game, error, root.string()) &&
            game.version == "01.00",
        "image0 homebrew external installation supported");
  install(root, "CUSA00004");
  check(installed_game("CUSA00004", game, error, root.string()) &&
            game.version.empty() && game.name == "CUSA00004" && !error.empty(),
        "missing metadata displays installed title but cannot match patches");
  ps4(root, "CUSA00004", "bad/version", "Bad version");
  check(installed_game("CUSA00004", game, error, root.string()) &&
            game.version.empty(),
        "malformed version cannot be used for matching");
  write(root.string() + "/system_data/priv/appmeta/CUSA00004/param.sfo",
        sfo({{"TITLE_ID", "CUSA00900"}, {"APP_VER", "01.09"}}));
  check(installed_game("CUSA00004", game, error, root.string()) &&
            game.version.empty(),
        "mismatched SFO TITLE_ID cannot establish compatibility");
  for (const std::string title : {"../CUSA00900", "/CUSA00900", "CUSA0090x",
                                  "CUSA00900/../PPSA12345", "NPXS12345", ""})
    check(!installed_game(title, game, error, root.string()),
          "invalid/untrusted Title ID refused");
  check(installed_games(root.string() + "/../").games.empty(),
        "path traversal filesystem root rejected");
  check(installed_games("relative/root").games.empty(),
        "relative filesystem root rejected");

  install(root, "CUSA00005");
  fs::create_directories(root.string() + "/system_data/priv/appmeta/CUSA00005");
  fs::create_symlink(root.string() + "/system_data/priv/appmeta/CUSA00900/param.sfo",
                    root.string() + "/system_data/priv/appmeta/CUSA00005/param.sfo");
  check(installed_game("CUSA00005", game, error, root.string()) &&
            game.version.empty(),
        "symlink metadata leaf refused");
  fs::create_directory_symlink(root.string() + "/user/app/CUSA00900",
                              root.string() + "/user/app/CUSA00006");
  check(!installed_game("CUSA00006", game, error, root.string()),
        "symlink application directory refused");
  fs::create_directories(root.string() + "/user/app/CUSA00007");
  fs::create_symlink(root.string() + "/user/app/CUSA00900/app.pkg",
                    root.string() + "/user/app/CUSA00007/app.pkg");
  check(!installed_game("CUSA00007", game, error, root.string()),
        "symlink install evidence refused");
  write(root.string() + "/user/app/CUSA00008/app.pkg", "");
  check(!installed_game("CUSA00008", game, error, root.string()),
        "empty package does not prove installation");

  const std::string sfo_path = root.string() + "/system_data/priv/appmeta/CUSA00004/param.sfo";
  const std::string valid = sfo({{"TITLE_ID", "CUSA00004"}, {"APP_VER", "01.09"}});
  std::vector<std::string> invalid;
  invalid.push_back("garbage");
  invalid.push_back(valid.substr(0, 19));
  for (const auto &[offset, value] : std::map<size_t, uint32_t>{
           {8, 0xfffffff0}, {12, 0xfffffff0}, {16, 0xffffffff},
           {24, 0xffffffff}, {28, 0}, {32, 0xffffffff}}) {
    std::string broken = valid;
    put32(broken, offset, value);
    invalid.push_back(std::move(broken));
  }
  std::string missing_nul = valid;
  size_t keys = 20 + 2 * 16;
  std::fill(missing_nul.begin() + keys, missing_nul.end(), 'x');
  invalid.push_back(std::move(missing_nul));
  invalid.push_back(std::string(256 * 1024 + 1, 'x'));
  for (const auto &broken : invalid) {
    write(sfo_path, broken);
    check(installed_game("CUSA00004", game, error, root.string()) &&
              game.version.empty(),
          "malformed SFO fails closed without memory overread");
  }
  // Mutate every byte of a compact SFO: sanitizer verifies all table/key/value
  // boundary handling. A mutation may remain valid, but cannot change title.
  for (size_t i = 0; i < valid.size(); ++i) {
    std::string changed = valid;
    changed[i] ^= static_cast<char>(0xff);
    write(sfo_path, changed);
    check(installed_game("CUSA00004", game, error, root.string()) &&
              game.title == "CUSA00004" &&
              (game.version.empty() || game.version == "01.09"),
          "mutated SFO remains bounded and cannot spoof title/version");
  }
  const std::string json_path = root.string() + "/system_data/priv/appmeta/PPSA12345/param.json";
  const std::vector<std::string> bad_json = {
      "{", "[]", R"({"titleId":"PPSA99999","contentVersion":"01.005.000"})",
      R"({"titleId":"PPSA12345","version":"01.005.000"})",
      R"({"titleId":"PPSA12345","contentVersion":"01.005"})",
      R"({"titleId":"PPSA12345","contentVersion":"../01.005.000"})",
      R"({"titleId":"PPSA12345","titleId":"PPSA99999","contentVersion":"01.005.000"})",
      R"({"titleId":"PPSA12345\u0000invalid","contentVersion":"01.005.000"})",
      R"({"titleId":"PPSA12345","contentVersion":"01.005.000"}junk)",
      std::string(40, '[') + std::string(40, ']'),
      std::string(256 * 1024 + 1, 'x')};
  for (const auto &broken : bad_json) {
    write(json_path, broken);
    check(installed_game("PPSA12345", game, error, root.string()) &&
              game.version.empty(),
          "corrupt/ambiguous/unsupported PS5 metadata version blocked");
  }
  ps4(root, "CUSA00004", "01.09", std::string("Bad\nname"));
  check(installed_game("CUSA00004", game, error, root.string()) &&
            game.name == "CUSA00004" && game.version == "01.09",
        "control-byte display name replaced by safe Title ID");
  ps4(root, "CUSA00004", "01.09", std::string("Bad\xffname"));
  check(installed_game("CUSA00004", game, error, root.string()) &&
            game.name == "CUSA00004",
        "malformed UTF-8 display name replaced by safe Title ID");
  const auto before = installed_games(root.string());
  const auto after = installed_games(root.string());
  check(before.games.size() == after.games.size(), "repeated scans stable");
  check(!before.warnings.empty(), "unknown versions produce meaningful warnings");
  check(std::is_sorted(before.games.begin(), before.games.end(),
                       [](const auto &a, const auto &b) {
                         return a.name == b.name ? a.title < b.title : a.name < b.name;
                       }), "picker results sorted by game name");
  fs::remove_all(root);
  std::cout << "Installed-game catalog: " << checks << " checks passed\n";
}
