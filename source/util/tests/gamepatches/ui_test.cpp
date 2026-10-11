#include "game_patches.hpp"
#include "onpress.hpp"
#include "settings_page_refresh.hpp"
#include "onion_cjson.hpp"
#include <cassert>
#include <iostream>
static onion::shellui::settings::PageSource source;
static std::string rendered, key(64, 'a');
static bool fail = false;
static int saves = 0, reloads = 0;
IPC_Client::IPC_Client(bool util, int timeout):util_daemon(util),util_daemon_(util),socket_fd_(-1),recv_timeout_ms_(timeout){}
IPC_Client &IPC_Client::getInstance(bool) { static IPC_Client instance(true, 1000); return instance; }
bool IPC_Client::IPCSendCommand(DaemonCommands cmd, std::string &reply, std::string request) {
 if(fail)return false;onion_cjson::Root j(request);assert(j);
 if(cmd==BREW_UTIL_PATCH_GAMES)reply=R"({"status":"Choose a game","collection":"ready","games":[{"title":"CUSA00900","name":"Bloodborne","version":"01.09"}],"next":-1})";
 else if(cmd==BREW_UTIL_PATCH_LIST){if(onion_cjson::bool_item(j.get(),"refresh",false))++reloads;assert(std::string(onion_cjson::string_item(j.get(),"title",""))=="CUSA00900");reply="{\"session\":\"selection-token\",\"title\":\"CUSA00900\",\"version\":\"01.09\",\"patches\":[{\"key\":\""+key+"\",\"name\":\"60 FPS\",\"supported\":true,\"enabled\":false}],\"next\":-1}";}
 else if(cmd==BREW_UTIL_PATCH_TOGGLE){assert(std::string(onion_cjson::string_item(j.get(),"session",""))=="selection-token");++saves;reply=R"({"status":"Saved ON; restart game required"})";}
 else assert(false);return true;
}
extern "C" void onion_notify(int,const char*,...){}
namespace onion::shellui::settings {
std::string publish(const ps5ui::Node &model,PageSource callback){source=callback;rendered=ps5ui::build_document(model);return rendered;}
void refresh(MonoObject*){ps5ui::Node model;source(model);rendered=ps5ui::build_document(model);}
}
int main(){std::string xml;generate_game_patches_xml(xml);assert(xml.find("Bloodborne")!=std::string::npos);assert(xml.find("id_gp_game_CUSA00900")!=std::string::npos);
 OnPressContext ctx;ctx.id="id_gp_game_CUSA00900";assert(game_patch_press(ctx));assert(rendered.find("60 FPS")!=std::string::npos);assert(rendered.find("Choose another game")!=std::string::npos);
 assert(reloads==1);ctx.id="id_gp_refresh";assert(game_patch_press(ctx));assert(reloads==2);
 std::string value;assert(game_patch_value("id_gp_toggle_"+key,value)&&value=="0");ctx.id="id_gp_toggle_"+key;ctx.value="1";assert(game_patch_press(ctx));assert(saves==1&&!ctx.dirty&&reloads==2);
 ctx.id="id_gp_choose_game";assert(game_patch_press(ctx));assert(rendered.find("Bloodborne")!=std::string::npos);fail=true;ctx.id="id_gp_refresh";game_patch_press(ctx);assert(rendered.find("unavailable")!=std::string::npos);
 assert(!game_patch_value("unknown",value));ctx.id="unknown";assert(!game_patch_press(ctx));std::cout<<"PASS: native installed-game navigation, persistent toggles, back and service failure\n";}
