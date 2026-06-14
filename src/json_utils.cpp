#include "json_utils.h"

#include <cctype>

std::optional<std::string> GetJsonValue(const std::string& json, const std::string& key)
{
    bool in_string = false;
    bool escaped = false;
    int object_depth = 0;

    for (size_t i = 0; i < json.size(); ++i) {
        char c = json[i];

        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }

        if (c == '"') {
            // Only consider keys at the outermost object depth (depth == 1)
            if (object_depth != 1) {
                in_string = true;
                continue;
            }

            // Read the quoted string
            size_t key_start = i + 1;
            size_t key_end = key_start;
            bool key_escaped = false;
            while (key_end < json.size()) {
                char d = json[key_end];
                if (key_escaped) {
                    key_escaped = false;
                } else if (d == '\\') {
                    key_escaped = true;
                } else if (d == '"') {
                    break;
                }
                ++key_end;
            }

            if (key_end >= json.size()) return std::nullopt;
            std::string found_key = json.substr(key_start, key_end - key_start);
            i = key_end;
            in_string = false;

            // Skip whitespace until colon
            size_t pos = i + 1;
            while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
                ++pos;
            }
            if (pos >= json.size() || json[pos] != ':') continue;
            ++pos;

            if (found_key != key) continue;

            // Skip whitespace after the colon
            while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
                ++pos;
            }
            if (pos >= json.size()) return std::nullopt;

            // Handle quoted string values
            if (json[pos] == '"') {
                size_t value_start = pos + 1;
                size_t value_end = value_start;
                bool value_escaped = false;
                while (value_end < json.size()) {
                    char d = json[value_end];
                    if (value_escaped) {
                        value_escaped = false;
                    } else if (d == '\\') {
                        value_escaped = true;
                    } else if (d == '"') {
                        break;
                    }
                    ++value_end;
                }
                if (value_end >= json.size()) return std::nullopt;

                std::string result;
                result.reserve(value_end - value_start);
                for (size_t j = value_start; j < value_end; ++j) {
                    if (json[j] == '\\' && j + 1 < value_end) {
                        ++j;
                        result.push_back(json[j]);
                    } else {
                        result.push_back(json[j]);
                    }
                }
                return result;
            }

            // Handle unquoted values
            size_t value_start = pos;
            size_t value_end = value_start;
            while (value_end < json.size() && json[value_end] != ',' && json[value_end] != '}' &&
                   json[value_end] != ']' && !std::isspace(static_cast<unsigned char>(json[value_end]))) {
                ++value_end;
            }
            if (value_end == value_start) return std::nullopt;
            return json.substr(value_start, value_end - value_start);
        }

        if (c == '{') {
            ++object_depth;
        } else if (c == '}') {
            --object_depth;
        }
    }

    return std::nullopt;
}