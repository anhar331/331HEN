#pragma once
#include <string>
void generate_game_patches_xml(std::string &xml);
bool game_patch_value(const std::string &id, std::string &value);
struct OnPressContext;
bool game_patch_press(OnPressContext &ctx);
