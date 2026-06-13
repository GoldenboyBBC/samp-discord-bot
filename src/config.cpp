#include "config.h"

#include <fstream>
#include <sstream>
#include <string>

std::string GetTokenFromConfig()
{
    std::ifstream file("server.cfg");
    if (!file.is_open()) return {};

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        if (!(iss >> key)) continue;

        if (key != "discord_token" && key != "DISCORD_TOKEN") continue;

        std::string token;
        if (!(iss >> token)) continue;

        if (!token.empty() && token.front() == '"') token.erase(token.begin());
        if (!token.empty() && token.back()  == '"') token.pop_back();

        return token;
    }
    return {};
}