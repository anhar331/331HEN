#pragma once
#include <cstdint>
#include <string>
namespace hen331::patches {
void initialize();
void tick();
std::string games(size_t offset);
std::string list(size_t offset, const std::string &title = "");
std::string toggle(const std::string &key, const std::string &session, bool enabled);
std::string download();
std::string launch(int pid, int appid, uint64_t generation, const std::string &title, const std::string &version);
std::string ended(int pid, uint64_t generation = 0);
} // namespace hen331::patches
