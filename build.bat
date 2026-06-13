@echo off
g++ -m32 -DHAVE_STDINT_H -shared ^
    plugin.cpp ^
    src/discord_bot.cpp ^
    src/config.cpp ^
    src/json_utils.cpp ^
    src/rest_client.cpp ^
    src/channel_store.cpp ^
    src/message_store.cpp ^
    src/natives/bot.cpp ^
    src/natives/messages.cpp ^
    samp-plugin-sdk/amxplugin.cpp ^
    -o discord_connector.dll ^
    -I samp-plugin-sdk ^
    -lcurl -lssl -lcrypto -lws2_32 ^
    -Wl,--kill-at plugin.def

if %errorlevel% equ 0 (
    move /Y discord_connector.dll "F:\SAMP 0.3e Server\plugins\"
    echo Build successful and moved.
) else (
    echo Build failed.
)