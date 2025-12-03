#include "steam/steam_networking_manager.h"
#include "steam/steam_room_manager.h"
#include "steam/steam_utils.h"
#include "tcp_server.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using boost::asio::ip::tcp;

// Global variables
std::vector<HSteamNetConnection> connections;
std::mutex connectionsMutex;
int localPort = 0;
std::unique_ptr<TCPServer> server;
std::atomic<bool> isRunning(true);
std::atomic<bool> monitorMode(false);

// Command queue
std::queue<std::string> commandQueue;
std::mutex commandQueueMutex;

void inputThreadFunc() {
    std::string line;
    while (isRunning) {
        if (std::getline(std::cin, line)) {
            std::lock_guard<std::mutex> lock(commandQueueMutex);
            commandQueue.push(line);
        }
    }
}

void printHelp() {
    std::cout << "\nAvailable Commands:\n";
    std::cout << "  host <port>       - Host a lobby (Port required)\n";
    std::cout << "  join <LobbyID>    - Join a lobby\n";
    std::cout << "  disconnect        - Leave lobby and stop server\n";
    std::cout << "  friends           - List Steam friends\n";
    std::cout << "  invite <name>     - Invite friend (fuzzy match)\n";
    std::cout << "  status            - Show status once\n";
    std::cout << "  monitor [on/off]  - Toggle real-time status monitor\n";
    std::cout << "  help              - Show this help\n";
    std::cout << "  quit / exit       - Exit application\n";
    std::cout << "> " << std::flush;
}

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void printStatus(SteamNetworkingManager& steamManager, SteamRoomManager& roomManager) {
    if (monitorMode) {
        clearScreen();
        std::cout << "=== Real-time Monitor (Type 'monitor off' to stop) ===\n\n";
    }

    if (steamManager.isHost()) {
        std::cout << "[HOST] Hosting Lobby. Local Port: " << localPort << "\n";
    } else if (steamManager.isConnected()) {
        std::cout << "[CLIENT] Connected to Lobby.\n";
    } else {
        std::cout << "[STATUS] Not connected.\n";
        return;
    }

    CSteamID lobbyID = roomManager.getCurrentLobby();
    if (lobbyID.IsValid()) {
        std::cout << "Lobby ID: " << lobbyID.ConvertToUint64() << "\n";
        std::cout << "Members:\n";
        
        std::vector<CSteamID> members = roomManager.getLobbyMembers();
        CSteamID mySteamID = SteamUser()->GetSteamID();
        CSteamID hostSteamID = steamManager.getHostSteamID();

        printf("%-20s %-10s %-20s\n", "Name", "Ping(ms)", "Relay Info");
        printf("--------------------------------------------------\n");

        for (const auto& memberID : members) {
            const char* name = SteamFriends()->GetFriendPersonaName(memberID);
            int ping = 0;
            std::string relayInfo = "-";

            if (memberID == mySteamID) {
                printf("%-20s %-10s %-20s\n", name, "-", "-");
            } else {
                if (steamManager.isHost()) {
                    // Host logic to find ping
                    std::lock_guard<std::mutex> lockConn(connectionsMutex);
                    for (const auto& conn : steamManager.getConnections()) {
                        SteamNetConnectionInfo_t info;
                        if (steamManager.getInterface()->GetConnectionInfo(conn, &info)) {
                            if (info.m_identityRemote.GetSteamID() == memberID) {
                                ping = steamManager.getConnectionPing(conn);
                                relayInfo = steamManager.getConnectionRelayInfo(conn);
                                break;
                            }
                        }
                    }
                } else {
                    // Client logic
                    if (memberID == hostSteamID) {
                        ping = steamManager.getHostPing();
                        if (steamManager.getConnection() != k_HSteamNetConnection_Invalid) {
                            relayInfo = steamManager.getConnectionRelayInfo(steamManager.getConnection());
                        }
                    }
                }
                
                if (relayInfo != "-") {
                    printf("%-20s %-10d %-20s\n", name, ping, relayInfo.c_str());
                } else {
                    printf("%-20s %-10s %-20s\n", name, "-", "-");
                }
            }
        }
    }
    
    if (server) {
        std::cout << "\nTCP Server Port: 8888 | Clients: " << server->getClientCount() << "\n";
    }
    
    if (!monitorMode) {
        std::cout << "\n> " << std::flush;
    }
}

int main() {
    // Initialize Steam API
    if (!SteamAPI_Init()) {
        std::cerr << "Failed to initialize Steam API" << std::endl;
        return 1;
    }

    boost::asio::io_context io_context;
    auto work_guard = boost::asio::make_work_guard(io_context);
    std::thread io_thread([&io_context]() { io_context.run(); });

    // Initialize Managers
    SteamNetworkingManager steamManager;
    if (!steamManager.initialize()) {
        std::cerr << "Failed to initialize Steam Networking Manager" << std::endl;
        SteamAPI_Shutdown();
        return 1;
    }

    SteamRoomManager roomManager(&steamManager);
    
    // Set dependencies
    steamManager.setMessageHandlerDependencies(io_context, server, localPort);
    steamManager.startMessageHandler();

    std::cout << "ConnectTool CLI Started.\n";
    printHelp();

    // Start input thread
    std::thread inputThread(inputThreadFunc);
    inputThread.detach();

    auto lastStatusTime = std::chrono::steady_clock::now();

    while (isRunning) {
        SteamAPI_RunCallbacks();
        steamManager.update();

        // Process commands
        std::string command;
        {
            std::lock_guard<std::mutex> lock(commandQueueMutex);
            if (!commandQueue.empty()) {
                command = commandQueue.front();
                commandQueue.pop();
            }
        }

        if (!command.empty()) {
            std::stringstream ss(command);
            std::string cmd;
            ss >> cmd;

            if (cmd == "quit" || cmd == "exit") {
                isRunning = false;
            } else if (cmd == "help") {
                printHelp();
            } else if (cmd == "host") {
                int port = 0;
                if (ss >> port) {
                    localPort = port;
                    roomManager.startHosting();
                    std::cout << "Hosting lobby on local port " << localPort << "...\n";
                    monitorMode = true; // Auto-enable monitor
                } else {
                    std::cout << "Usage: host <port>\n";
                }
            } else if (cmd == "join") {
                uint64 lobbyIDVal;
                if (ss >> lobbyIDVal) {
                    if (steamManager.joinHost(lobbyIDVal)) {
                        // Start TCP Server
                        server = std::make_unique<TCPServer>(8888, &steamManager);
                        if (!server->start()) {
                            std::cerr << "Failed to start TCP server\n";
                        } else {
                            std::cout << "Joined lobby " << lobbyIDVal << ". TCP Server started on 8888.\n";
                            monitorMode = true; // Auto-enable monitor
                        }
                    } else {
                        std::cout << "Failed to join lobby.\n";
                    }
                } else {
                    std::cout << "Usage: join <LobbyID>\n";
                }
            } else if (cmd == "disconnect") {
                roomManager.leaveLobby();
                steamManager.disconnect();
                if (server) {
                    server->stop();
                    server.reset();
                }
                monitorMode = false;
                std::cout << "Disconnected.\n";
            } else if (cmd == "friends") {
                std::cout << "Friends List:\n";
                for (const auto& friendPair : SteamUtils::getFriendsList()) {
                    std::cout << " - " << friendPair.second << " (" << friendPair.first.ConvertToUint64() << ")\n";
                }
            } else if (cmd == "invite") {
                std::string filter;
                ss >> filter;
                if (filter.empty()) {
                    std::cout << "Usage: invite <name_part>\n";
                } else {
                    std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
                    bool found = false;
                    for (const auto& friendPair : SteamUtils::getFriendsList()) {
                        std::string name = friendPair.second;
                        std::string nameLower = name;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                        
                        if (nameLower.find(filter) != std::string::npos) {
                            if (SteamMatchmaking()) {
                                SteamMatchmaking()->InviteUserToLobby(roomManager.getCurrentLobby(), friendPair.first);
                                std::cout << "Invited " << name << "\n";
                                found = true;
                            }
                        }
                    }
                    if (!found) std::cout << "No friend found matching '" << filter << "'\n";
                }
            } else if (cmd == "status") {
                printStatus(steamManager, roomManager);
            } else if (cmd == "monitor") {
                std::string arg;
                ss >> arg;
                if (arg == "on") monitorMode = true;
                else if (arg == "off") monitorMode = false;
                else std::cout << "Usage: monitor [on/off]\n";
            } else {
                std::cout << "Unknown command. Type 'help' for list.\n";
            }
            
            if (!monitorMode) std::cout << "> " << std::flush;
        }

        // Real-time monitor update
        if (monitorMode) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastStatusTime).count() >= 1) {
                printStatus(steamManager, roomManager);
                lastStatusTime = now;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Cleanup
    steamManager.stopMessageHandler();
    if (server) server->stop();
    
    work_guard.reset();
    io_context.stop();
    if (io_thread.joinable()) io_thread.join();

    steamManager.shutdown();
    SteamAPI_Shutdown();

    return 0;
}