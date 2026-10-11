// 331HEN installed-game metadata discovery. GPL-3.0-or-later.
#pragma once
#include <string>
#include <vector>

namespace hen331::patches {
struct InstalledGame {
  std::string title, name, version, platform, metadata_path, install_path;
};
struct InstalledGameCatalog {
  std::vector<InstalledGame> games;
  std::vector<std::string> warnings;
};

// Read-only discovery: cached appmeta without a regular installed app.pkg or
// homebrew eboot.bin is never sufficient. An empty version means the actual
// installed metadata could not establish a compatible version; do not patch it.
// filesystem_root prefixes the PS5 absolute paths for isolated host fixtures.
InstalledGameCatalog installed_games(const std::string &filesystem_root = "");
bool installed_game(const std::string &title, InstalledGame &out,
                    std::string &error,
                    const std::string &filesystem_root = "");
} // namespace hen331::patches
