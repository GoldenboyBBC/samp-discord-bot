// src/channel_store.cpp
#include "channel_store.h"

#include <unordered_map>
#include <map>
#include <string>

namespace channel_store {

// Keyed by Discord Snowflake ID
static std::unordered_map<std::string, Channel> s_channels_by_id;
// Keyed by Pawn ID for quick lookup
static std::map<ChannelId, Channel> s_channels_by_pawn_id;
static ChannelId s_next_pawn_id = 1;

ChannelId Add(const Channel& channel) {
    // Check if this Discord channel already exists
    auto it = s_channels_by_id.find(channel.id);
    if (it != s_channels_by_id.end()) {
        return it->second.pawn_id;  // Already exists, return existing ID
    }

    // Assign new pawn ID
    ChannelId new_id = s_next_pawn_id++;
    Channel ch = channel;
    ch.pawn_id = new_id;

    s_channels_by_id[ch.id] = ch;
    s_channels_by_pawn_id[new_id] = ch;

    return new_id;
}

void Remove(const std::string& id) {
    auto it = s_channels_by_id.find(id);
    if (it != s_channels_by_id.end()) {
        ChannelId pawn_id = it->second.pawn_id;
        s_channels_by_id.erase(it);
        s_channels_by_pawn_id.erase(pawn_id);
    }
}

const Channel* FindById(const std::string& id) {
    auto it = s_channels_by_id.find(id);
    return (it != s_channels_by_id.end()) ? &it->second : nullptr;
}

const Channel* FindByName(const std::string& name) {
    for (auto& [id, ch] : s_channels_by_id) {
        if (ch.name == name) return &ch;
    }
    return nullptr;
}

const Channel* FindByPawnId(ChannelId pawn_id) {
    auto it = s_channels_by_pawn_id.find(pawn_id);
    return (it != s_channels_by_pawn_id.end()) ? &it->second : nullptr;
}

} // namespace channel_store