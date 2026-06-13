#include "messages.h"
#include "../channel_store.h"
#include "../message_store.h"
#include "../rest_client.h"
#include "../json_utils.h"

#include <string>
#include <iostream>

namespace natives {
namespace messages {

// DCC_FindChannelByName(const name[])
cell AMX_NATIVE_CALL FindChannelByName(AMX* amx, cell* params) {
    if (params[0] < 1 * sizeof(cell)) return 0;
    char* name = nullptr;
    amx_StrParam(amx, params[1], name);
    if (!name) return 0;
    const Channel* ch = channel_store::FindByName(name);
    return ch ? 1 : 0;
}

// DCC_FindChannelById(const id[])
cell AMX_NATIVE_CALL FindChannelById(AMX* amx, cell* params) {
    if (params[0] < 1 * sizeof(cell)) return 0;
    char* id = nullptr;
    amx_StrParam(amx, params[1], id);
    if (!id) return 0;
    const Channel* ch = channel_store::FindById(id);
    return ch ? 1 : 0;
}

// DCC_SendChannelMessage(const channel_id[], const message[])
cell AMX_NATIVE_CALL SendChannelMessage(AMX* amx, cell* params) {
    if (params[0] < 2 * sizeof(cell)) return 0;

    char* channel_id = nullptr;
    amx_StrParam(amx, params[1], channel_id);
    if (!channel_id) return 0;

    char* text = nullptr;
    amx_StrParam(amx, params[2], text);
    if (!text) return 0;

    const Channel* ch = channel_store::FindById(channel_id);
    if (!ch) {
        std::cerr << "[messages] Channel not found: " << channel_id << '\n';
        return 0;
    }

    std::string content(text);
    std::string escaped;
    for (char c : content) {
        if (c == '"' || c == '\\') escaped += '\\';
        escaped += c;
    }

    std::string body = "{\"content\":\"" + escaped + "\"}";
    std::string response = rest::Post("/channels/" + ch->id + "/messages", body);

    auto msg_id = GetJsonValue(response, "id");
    if (!msg_id) {
        std::cerr << "[messages] Send failed, response: " << response << '\n';
        return 0;
    }

    Message msg;
    msg.id         = *msg_id;
    msg.channel_id = ch->id;
    msg.content    = content;
    msg.author_id  = "";

    return static_cast<cell>(message_store::Add(msg));
}

// DCC_DeleteMessage(message_handle)
cell AMX_NATIVE_CALL DeleteMessage(AMX* amx, cell* params) {
    if (params[0] < 1 * sizeof(cell)) return 0;
    MessageHandle handle = static_cast<MessageHandle>(params[1]);
    const Message* msg = message_store::FindByHandle(handle);
    if (!msg) return 0;
    rest::Delete("/channels/" + msg->channel_id + "/messages/" + msg->id);
    message_store::Remove(handle);
    return 1;
}

// DCC_EditMessage(message_handle, const content[])
cell AMX_NATIVE_CALL EditMessage(AMX* amx, cell* params) {
    if (params[0] < 2 * sizeof(cell)) return 0;
    MessageHandle handle = static_cast<MessageHandle>(params[1]);
    const Message* msg = message_store::FindByHandle(handle);
    if (!msg) return 0;

    char* text = nullptr;
    amx_StrParam(amx, params[2], text);
    if (!text) return 0;

    std::string content(text);
    std::string escaped;
    for (char c : content) {
        if (c == '"' || c == '\\') escaped += '\\';
        escaped += c;
    }

    std::string body = "{\"content\":\"" + escaped + "\"}";
    rest::Patch("/channels/" + msg->channel_id + "/messages/" + msg->id, body);
    return 1;
}

// DCC_GetMessageContent(message_handle, dest[], max_size)
cell AMX_NATIVE_CALL GetMessageContent(AMX* amx, cell* params) {
    if (params[0] < 3 * sizeof(cell)) return 0;
    MessageHandle handle = static_cast<MessageHandle>(params[1]);
    const Message* msg = message_store::FindByHandle(handle);
    if (!msg) return 0;

    cell* dest = nullptr;
    amx_GetAddr(amx, params[2], &dest);
    int max_size = static_cast<int>(params[3]);
    amx_SetString(dest, msg->content.c_str(), 0, 0, max_size);
    return 1;
}

const AMX_NATIVE_INFO kNatives[] = {
    {"DCC_FindChannelByName",  FindChannelByName},
    {"DCC_FindChannelById",    FindChannelById},
    {"DCC_SendChannelMessage", SendChannelMessage},
    {"DCC_DeleteMessage",      DeleteMessage},
    {"DCC_EditMessage",        EditMessage},
    {"DCC_GetMessageContent",  GetMessageContent},
    {nullptr, nullptr}
};

} // namespace messages
} // namespace natives