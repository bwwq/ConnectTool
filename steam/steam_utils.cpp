#include "steam_utils.h"
#include <iostream>

std::vector<std::pair<CSteamID, std::string>> SteamUtils::getFriendsList() {
    std::vector<std::pair<CSteamID, std::string>> friendsList;
    int friendCount = SteamFriends()->GetFriendCount(k_EFriendFlagAll);
    for (int i = 0; i < friendCount; ++i) {
        CSteamID friendID = SteamFriends()->GetFriendByIndex(i, k_EFriendFlagAll);
        const char* name = SteamFriends()->GetFriendPersonaName(friendID);
        friendsList.push_back({friendID, name});
    }
    return friendsList;
}
