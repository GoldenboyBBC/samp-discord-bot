#include "json_utils.h"

std::optional<std::string> GetJsonValue(const std::string& json, const std::string& key)
{
    const std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return std::nullopt;

    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return std::nullopt;

    pos = json.find_first_not_of(" \t\n\r\"", pos + 1);
    if (pos == std::string::npos) return std::nullopt;

    size_t end = json.find_first_of(" ,}\"", pos);
    return (end == std::string::npos) ? json.substr(pos)
                                      : json.substr(pos, end - pos);
}