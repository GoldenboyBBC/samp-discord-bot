// src/rest_client.h
#pragma once

#include <string>

namespace rest {

// Must be called once before any requests, passing your bot token.
void Init(const std::string& bot_token);

// HTTP methods — all return the raw JSON response body, or empty string on failure.
std::string Get   (const std::string& endpoint);
std::string Post  (const std::string& endpoint, const std::string& json_body);
std::string Patch (const std::string& endpoint, const std::string& json_body);
std::string Delete(const std::string& endpoint);

} // namespace rest