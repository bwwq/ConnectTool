#pragma once
#include <vector>
#include <string>
#include <steam_api.h>

class SteamUtils {
public:
    static std::vector<std::pair<CSteamID, std::string>> getFriendsList();
};