#pragma once

#include <string>

/**
 * Reads "discord_token" / "DISCORD_TOKEN" from server.cfg.
 * Returns an empty string when the key is missing or the file cannot be opened.
 */
std::string GetTokenFromConfig();