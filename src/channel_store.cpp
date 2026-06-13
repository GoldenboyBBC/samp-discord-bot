// src/channel_store.cpp
#include "channel_store.h"

#include <unordered_map>
#include <string>

namespace channel_store {

// Keyed by channel ID
static std::unordered_map<std::string, Channel> s_channels;

void Add(const Channel& channel) {
    s_channels[channel.id] = channel;
}

void Remove(const std::string& id) {
    s_channels.erase(id);
}

const Channel* FindById(const std::string& id) {
    auto it = s_channels.find(id);
    return (it != s_channels.end()) ? &it->second : nullptr;
}

const Channel* FindByName(const std::string& name) {
    for (auto& [id, ch] : s_channels) {
        if (ch.name == name) return &ch;
    }
    return nullptr;
}

} // namespace channel_store