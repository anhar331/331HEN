#pragma once
#include <string>
namespace hen331::patches {
void initialize();
void tick();
std::string list(size_t offset);
std::string toggle(const std::string &key, const std::string &session,
                   bool enabled);
std::string download();
std::string launch(int pid, const std::string &title);
} // namespace hen331::patches
