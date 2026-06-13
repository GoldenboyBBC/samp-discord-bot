#pragma once

#include <optional>
#include <string>

/**
 * Extracts a primitive (non-nested) JSON value for the given key.
 * Returns std::nullopt when the key is absent or malformed.
 */
std::optional<std::string> GetJsonValue(const std::string& json, const std::string& key);