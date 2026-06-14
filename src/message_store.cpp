// src/message_store.cpp
#include "message_store.h"

namespace message_store {

static std::unordered_map<MessageHandle, Message> s_messages;
static std::unordered_map<std::string, MessageHandle> s_id_to_handle;
static MessageHandle s_next_handle = 1;

MessageHandle Add(const Message& message) {
    MessageHandle handle = s_next_handle++;
    s_messages[handle]            = message;
    s_id_to_handle[message.id]    = handle;
    return handle;
}

void Remove(MessageHandle handle) {
    auto it = s_messages.find(handle);
    if (it == s_messages.end()) return;
    s_id_to_handle.erase(it->second.id);
    s_messages.erase(it);
}

const Message* FindByHandle(MessageHandle handle) {
    auto it = s_messages.find(handle);
    return (it != s_messages.end()) ? &it->second : nullptr;
}

const Message* FindById(const std::string& id) {
    auto it = s_id_to_handle.find(id);
    if (it == s_id_to_handle.end()) return nullptr;
    return FindByHandle(it->second);
}

MessageHandle FindHandleById(const std::string& id) {
    auto it = s_id_to_handle.find(id);
    if (it == s_id_to_handle.end()) return INVALID_MESSAGE_HANDLE;
    return it->second;
}

} // namespace message_store