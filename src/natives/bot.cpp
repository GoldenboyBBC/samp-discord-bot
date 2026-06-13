#include "bot.h"
#include "../bot_instance.h"

#include <string>

static const char* StatusString(int status) {
    switch (status) {
        case 1: return "online";
        case 2: return "idle";
        case 3: return "dnd";
        case 4: return "invisible";
        default: return "online";
    }
}

static int s_status = 1;
static std::string s_activity;

static void SendPresenceUpdate() {
    std::string activity_block = "[]";
    if (!s_activity.empty())
        activity_block = "[{\"name\":\"" + s_activity + "\",\"type\":0}]";

    std::string payload =
        "{\"op\":3,\"d\":{"
        "\"status\":\"" + std::string(StatusString(s_status)) + "\","
        "\"activities\":" + activity_block + ","
        "\"afk\":false,"
        "\"since\":null}}";

    g_bot->EnqueuePresenceUpdate(payload);
}

namespace natives {
namespace bot {

cell AMX_NATIVE_CALL SetBotPresenceStatus(AMX* amx, cell* params) {
    s_status = static_cast<int>(params[1]);
    SendPresenceUpdate();
    return 1;
}

cell AMX_NATIVE_CALL SetBotActivity(AMX* amx, cell* params) {
    char* name = nullptr;
    amx_StrParam(amx, params[1], name);
    s_activity = name ? name : "";
    SendPresenceUpdate();
    return 1;
}

cell AMX_NATIVE_CALL GetBotPresenceStatus(AMX* amx, cell* params) {
    return static_cast<cell>(s_status);
}

const AMX_NATIVE_INFO kNatives[] = {
    {"DCC_SetBotPresenceStatus", SetBotPresenceStatus},
    {"DCC_SetBotActivity",       SetBotActivity},
    {"DCC_GetBotPresenceStatus", GetBotPresenceStatus},
    {nullptr, nullptr}
};

} // namespace bot
} // namespace natives