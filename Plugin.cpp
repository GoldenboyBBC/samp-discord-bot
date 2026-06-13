#include <stdint.h>
#include <stddef.h>
#include "samp-plugin-sdk/plugincommon.h"
#include "samp-plugin-sdk/amx/amx.h"

#include "src/config.h"
#include "src/discord_bot.h"
#include "src/rest_client.h"

// Natives
#include "src/natives/bot.h"
#include "src/natives/messages.h"
#include <iostream>

// Defined by the SDK in amxplugin.cpp — we only declare it here.
extern void* pAMXFunctions;

DiscordBot* g_bot = nullptr; // extern-visible via src/bot_instance.h

extern "C" {

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports()
{
    return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void** ppData)
{
    pAMXFunctions = ppData[PLUGIN_DATA_AMX_EXPORTS];
    curl_global_init(CURL_GLOBAL_ALL);

    std::string token = GetTokenFromConfig();
    if (token.empty()) {
        std::cerr << "[DiscordBot] No token found in server.cfg — aborting.\n";
        return false;
    }

    rest::Init(token);
    g_bot = new DiscordBot(std::move(token));

    if (!g_bot->ConnectAndWaitUntilReady()) {
        delete g_bot;
        g_bot = nullptr;
        return false;
    }

    return true;
}

PLUGIN_EXPORT void PLUGIN_CALL Unload()
{
    delete g_bot;
    g_bot = nullptr;
    curl_global_cleanup();
}

PLUGIN_EXPORT int PLUGIN_CALL AmxLoad(AMX* amx)
{
    amx_Register(amx, natives::bot::kNatives,      -1);
    amx_Register(amx, natives::messages::kNatives, -1);
    return AMX_ERR_NONE;
}

PLUGIN_EXPORT int PLUGIN_CALL AmxUnload(AMX* /*amx*/)
{
    return AMX_ERR_NONE;
}

} // extern "C"