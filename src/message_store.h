// src/message_store.h
#pragma once

#include <string>
#include <unordered_map>

struct Message {
    std::string id;
    std::string channel_id;
    std::string author_id;
    std::string content;
};

// A handle is just an integer Pawn uses to reference a stored message
using MessageHandle = int;

namespace message_store {

// Stores a message and returns its handle
MessageHandle Add(const Message& message);

void Remove(MessageHandle handle);

// Returns nullptr if not found
const Message* FindByHandle(MessageHandle handle);
const Message* FindById    (const std::string& id);

} // namespace message_store