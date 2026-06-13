// src/channel_store.h
#pragma once

#include <string>
#include <unordered_map>

struct Channel {
    std::string id;
    std::string name;
    std::string guild_id;
    int         type;
};

namespace channel_store {

void   Add       (const Channel& channel);
void   Remove    (const std::string& id);

// Returns nullptr if not found
const Channel* FindById  (const std::string& id);
const Channel* FindByName(const std::string& name);

} // namespace channel_store