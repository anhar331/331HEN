#include "game_patches.hpp"
#include "onion_cjson.hpp"
#include "onpress.hpp"
#include "ps5_settings_ui.hpp"
#include "settings_page_refresh.hpp"
#include <map>
namespace {
std::map<std::string, std::string> values;
std::string session, selected_title;
size_t games_offset = 0;
bool refresh_requested = true;
bool model(ps5ui::Node &out) {
  ps5ui::Page page("id_game_patch_page", "Game Patches");
  values.clear();
  if (selected_title.empty()) {
    std::string reply;
    bool ok = IPC_Client::getInstance(true).IPCSendCommand(
        BREW_UTIL_PATCH_GAMES, reply,
        "{\"offset\":" + std::to_string(games_offset) + ",\"refresh\":" + (refresh_requested ? "true" : "false") + "}");
    onion_cjson::Root j(reply);
    if (!ok || !j) {
      page.label("id_gp_unavailable", "Installed game service unavailable");
    } else {
      refresh_requested = false;
      page.label("id_gp_status", onion_cjson::string_item(j.get(), "status", ""))
          .label("id_gp_collection", onion_cjson::string_item(j.get(), "collection", ""));
      const std::string warning = onion_cjson::string_item(j.get(), "warning", "");
      if (!warning.empty()) page.label("id_gp_warning", warning);
      cJSON *item = nullptr;
      cJSON_ArrayForEach(item, cJSON_GetObjectItem(j.get(), "games")) {
        std::string title = onion_cjson::string_item(item, "title", "");
        if (title.size() != 9 || title.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != std::string::npos)
          continue;
        std::string name = onion_cjson::string_item(item, "name", title.c_str());
        std::string version = onion_cjson::string_item(item, "version", "");
        page.button("id_gp_game_" + title, name, title + " / " +
                    (version.empty() ? "Version unavailable" : version));
      }
      int next = onion_cjson::int_item(j.get(), "next", -1);
      if (games_offset > 0) page.button("id_gp_previous", "Previous games");
      if (next > static_cast<int>(games_offset)) {
        values["id_gp_next"] = std::to_string(next);
        page.button("id_gp_next", "Next games");
      }
    }
    page.button("id_gp_refresh", "Refresh installed games");
    out = page.root();
    return ok && bool(j);
  }
  page.button("id_gp_choose_game", "Choose another game");
  std::string reply;
  bool ok = IPC_Client::getInstance(true).IPCSendCommand(
      BREW_UTIL_PATCH_LIST, reply, "{\"offset\":0,\"title\":\"" + selected_title + "\",\"refresh\":" + (refresh_requested ? "true" : "false") + "}");
  onion_cjson::Root first(reply);
  if (!ok || !first) {
    page.label("id_gp_unavailable", "Patch service unavailable",
               ps5ui::Style::Center);
    out = page.root();
    return false;
  }
  refresh_requested = false;
  session = onion_cjson::string_item(first.get(), "session", "");
  std::string title = onion_cjson::string_item(first.get(), "title", "");
  std::string version = onion_cjson::string_item(first.get(), "version", "");
  page.label("id_gp_game", title + " / " + version, ps5ui::Style::Center)
      .label("id_gp_status",
             onion_cjson::string_item(first.get(), "status", ""),
             ps5ui::Style::Center)
      .label("id_gp_collection",
             onion_cjson::string_item(first.get(), "collection", ""),
             ps5ui::Style::Center)
      .label("id_gp_launch_timing",
             "Startup effects: restart game required. Pre-entry timing is "
             "not guaranteed.",
             ps5ui::Style::Center)
      .button("id_gp_refresh", "Reload patch files and status");
  int offset = 0;
  for (unsigned batch = 0; batch < 128; ++batch) {
    if (offset > 0) {
      if (!IPC_Client::getInstance(true).IPCSendCommand(
              BREW_UTIL_PATCH_LIST, reply,
              "{\"offset\":" + std::to_string(offset) + ",\"title\":\"" + selected_title + "\"}"))
        break;
    }
    onion_cjson::Root j(reply);
    if (!j || session != onion_cjson::string_item(j.get(), "session", ""))
      break;
    cJSON *items = cJSON_GetObjectItem(j.get(), "patches");
    cJSON *item = nullptr;
    cJSON_ArrayForEach(item, items) {
      std::string key = onion_cjson::string_item(item, "key", "");
      if (key.size() != 64)
        continue;
      std::string id = "id_gp_toggle_" + key,
                  name = onion_cjson::string_item(item, "name", ""),
                  status = onion_cjson::string_item(item, "status", ""),
                  author = onion_cjson::string_item(item, "author", "");
      bool enabled = onion_cjson::bool_item(item, "enabled", false);
      values[id] = enabled ? "1" : "0";
      if (onion_cjson::bool_item(item, "supported", false))
        page.toggle(id, name, enabled, status,
                    "By " + author + ". " +
                        onion_cjson::string_item(item, "note", ""));
      else
        page.label("id_gp_unsupported_" + key, name + " — " + status);
    }
    int next = onion_cjson::int_item(j.get(), "next", -1);
    if (next < 0 || next <= offset)
      break;
    offset = next;
  }
  out = page.root();
  return true;
}
} // namespace
void generate_game_patches_xml(std::string &xml) {
  ps5ui::Node out;
  refresh_requested = true;
  model(out);
  xml = onion::shellui::settings::publish(out, model);
}
bool game_patch_value(const std::string &id, std::string &value) {
  auto it = values.find(id);
  if (it == values.end())
    return false;
  value = it->second;
  return true;
}
bool game_patch_press(OnPressContext &ctx) {
  if (ctx.id == "id_gp_choose_game" || ctx.id == "id_gp_previous" ||
      ctx.id == "id_gp_next" || ctx.id.starts_with("id_gp_game_")) {
    ctx.dirty = false;
    if (ctx.id == "id_gp_choose_game" || ctx.id.starts_with("id_gp_game_")) refresh_requested = true;
    if (ctx.id == "id_gp_choose_game") selected_title.clear();
    else if (ctx.id == "id_gp_previous") games_offset = games_offset >= 4 ? games_offset - 4 : 0;
    else if (ctx.id == "id_gp_next") {
      auto next = values.find(ctx.id);
      if (next != values.end()) games_offset = std::stoul(next->second);
    } else {
      std::string title = ctx.id.substr(11);
      if (title.size() == 9 && title.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") == std::string::npos)
        selected_title = title;
    }
    onion::shellui::settings::refresh(ctx.instance);
    return true;
  }
  if (ctx.id == "id_gp_refresh") {
    ctx.dirty = false;
    refresh_requested = true;
    onion::shellui::settings::refresh(ctx.instance);
    return true;
  }
  const std::string prefix = "id_gp_toggle_";
  if (!ctx.id.starts_with(prefix))
    return false;
  ctx.dirty = false;
  cJSON *j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "key", ctx.id.substr(prefix.size()).c_str());
  cJSON_AddStringToObject(j, "session", session.c_str());
  cJSON_AddBoolToObject(j, "enabled", ctx.value == "1");
  std::string reply;
  IPC_Client::getInstance(true).IPCSendCommand(BREW_UTIL_PATCH_TOGGLE, reply,
                                               onion_cjson::print_owned(j));
  onion_cjson::Root result(reply);
  onion_notify(true, "%s",
               onion_cjson::string_item(result.get(), "status",
                                        "Patch service unavailable"));
  onion::shellui::settings::refresh(ctx.instance);
  return true;
}
