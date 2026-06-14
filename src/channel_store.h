// src/channel_store.h
#pragma once

#include <string>
#include <unordered_map>
#include <map>

using ChannelId = int;  // Pawn-side channel ID
const ChannelId INVALID_CHANNEL_ID = 0;

struct Channel {
    ChannelId   pawn_id;  // Sequential ID for Pawn
    std::string id;       // Discord Snowflake ID
    std::string name;
    std::string guild_id;
    int         type;
};

namespace channel_store {

// Add a channel and return its pawn_id (0 if failed)
ChannelId Add(const Channel& channel);
void   Remove(const std::string& id);

// Returns nullptr if not found
const Channel* FindById  (const std::string& id);
const Channel* FindByName(const std::string& name);
const Channel* FindByPawnId(ChannelId pawn_id);

} // namespace channel_store