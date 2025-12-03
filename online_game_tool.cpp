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
    std::cout << "\n可用命令：\n";
    std::cout << "  host <端口>       - 主持大厅（必须指定端口）\n";
    std::cout << "  join <大厅ID>     - 加入大厅\n";
    std::cout << "  disconnect        - 离开大厅并停止服务器\n";
    std::cout << "  friends           - 列出 Steam 好友\n";
    std::cout << "  invite <名称>     - 邀请好友（模糊匹配）\n";
    std::cout << "  status            - 显示一次当前状态\n";
    std::cout << "  monitor [on/off]  - 开启/关闭实时状态监控\n";
    std::cout << "  help              - 显示此帮助信息\n";
    std::cout << "  quit / exit       - 退出应用程序\n";
    std::cout << "> " << std::flush;
}

void enableAnsi() {
#ifdef _WIN32
    // Enable ANSI escape codes for Output
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwModeOut = 0;
    GetConsoleMode(hOut, &dwModeOut);
    dwModeOut |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwModeOut);
    SetConsoleOutputCP(65001); // Ensure UTF-8

    // Disable Quick Edit Mode for Input (prevents freezing on click)
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD dwModeIn = 0;
    GetConsoleMode(hIn, &dwModeIn);
    dwModeIn &= ~ENABLE_QUICK_EDIT_MODE;
    dwModeIn |= ENABLE_EXTENDED_FLAGS; // Required to disable Quick Edit
    SetConsoleMode(hIn, dwModeIn);
#endif
}

void clearScreen() {
    // Move cursor to top-left (1,1)
    std::cout << "\033[1;1H"; 
}

// Suppress Steam API logs
extern "C" void __cdecl SteamAPIDebugTextHook(int nSeverity, const char *pchDebugText) {
    // Do nothing to suppress output
}

void printStatus(SteamNetworkingManager& steamManager, SteamRoomManager& roomManager) {
    if (monitorMode) {
        clearScreen();
        std::cout << "=== 实时监控（输入 'monitor off' 停止） ===\033[K\n\n";
    }

    if (steamManager.isHost()) {
        std::cout << "[主机] 正在主持大厅。本地端口：" << localPort << "\033[K\n";
    } else if (steamManager.isConnected()) {
        std::cout << "[客户端] 已连接到大厅。\033[K\n";
    } else {
        std::cout << "[状态] 未连接。\033[K\n";
        std::string lastError = steamManager.getLastError();
        if (!lastError.empty()) {
            std::cout << "[信息] " << lastError << "\033[K\n";
        }
        // If not connected and in monitor mode, we still want to clear the rest of the screen
        if (monitorMode) std::cout << "\033[J";
        return;
    }

    CSteamID lobbyID = roomManager.getCurrentLobby();
    if (lobbyID.IsValid()) {
        std::cout << "--------------------------------------------------\033[K\n";
        std::cout << "大厅 ID：" << lobbyID.ConvertToUint64() << "\033[K\n";
        std::cout << "--------------------------------------------------\033[K\n";
        std::cout << "成员列表：\033[K\n";
        
        std::vector<CSteamID> members = roomManager.getLobbyMembers();
        CSteamID mySteamID = SteamUser()->GetSteamID();
        CSteamID hostSteamID = steamManager.getHostSteamID();

        printf("%-20s %-10s %-20s\033[K\n", "名称", "延迟(ms)", "中继信息");
        printf("--------------------------------------------------\033[K\n");

        for (const auto& memberID : members) {
            const char* name = SteamFriends()->GetFriendPersonaName(memberID);
            int ping = 0;
            std::string relayInfo = "-";

            if (memberID == mySteamID) {
                printf("%-20s %-10s %-20s\033[K\n", name, "-", "-");
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
                    printf("%-20s %-10d %-20s\033[K\n", name, ping, relayInfo.c_str());
                } else {
                    printf("%-20s %-10s %-20s\033[K\n", name, "-", "-");
                }
            }
        }
    }
    
    if (server) {
        std::cout << "\nTCP 服务器端口：8888 | 客户端数：" << server->getClientCount() << "\033[K\n";
    }
    
    if (monitorMode) {
        // Clear from cursor to end of screen to remove any leftover text from previous frames
        std::cout << "\033[J";
    } else {
        std::cout << "\n> " << std::flush;
    }
}

int main(int argc, char* argv[]) {
    enableAnsi(); // Enable ANSI, UTF-8, and disable Quick Edit FIRST

    // Initialize Steam API
    if (!SteamAPI_Init()) {
        std::cerr << "初始化 Steam API 失败" << std::endl;
        return 1;
    }

    // Suppress Steam API warnings/logs
    SteamUtils()->SetWarningMessageHook(&SteamAPIDebugTextHook);

    boost::asio::io_context io_context;
    auto work_guard = boost::asio::make_work_guard(io_context);
    std::thread io_thread([&io_context]() { io_context.run(); });

    // Initialize Managers
    SteamNetworkingManager steamManager;
    if (!steamManager.initialize()) {
        std::cerr << "初始化 Steam Networking Manager 失败" << std::endl;
        SteamAPI_Shutdown();
        return 1;
    }

    SteamRoomManager roomManager(&steamManager);
    
    // Set dependencies
    steamManager.setMessageHandlerDependencies(io_context, server, localPort);
    steamManager.startMessageHandler();

    // Check for command line arguments (Steam Invite)
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "+connect_lobby" && i + 1 < argc) {
            uint64 lobbyIDVal = std::stoull(argv[i + 1]);
            std::cout << "检测到启动参数：加入大厅 " << lobbyIDVal << "\n";
            if (roomManager.joinLobby(lobbyIDVal)) {
                std::cout << "正在加入大厅 " << lobbyIDVal << "...\n";
                monitorMode = true;
            } else {
                std::cerr << "加入大厅请求失败\n";
            }
        }
    }

    std::cout << "ConnectTool 命令行工具已启动。\n";
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
            std::string cmd;
            std::string arg;

            // Helper lambda to check prefix and extract argument
            auto checkCommand = [&](const std::string& prefix) -> bool {
                if (command.rfind(prefix, 0) == 0) { // Starts with prefix
                    cmd = prefix;
                    if (command.length() > prefix.length()) {
                        arg = command.substr(prefix.length());
                        // Trim leading spaces
                        arg.erase(0, arg.find_first_not_of(" \t"));
                    }
                    return true;
                }
                return false;
            };

            if (command == "quit" || command == "exit") {
                isRunning = false;
            } else if (command == "help") {
                printHelp();
            } else if (checkCommand("host")) {
                int port = 0;
                try {
                    if (!arg.empty()) {
                        port = std::stoi(arg);
                        localPort = port;
                        roomManager.startHosting();
                        std::cout << "正在本地端口 " << localPort << " 主持大厅...\n";
                        monitorMode = true;
                    } else {
                        std::cout << "用法：host <端口>\n";
                    }
                } catch (...) {
                    std::cout << "无效端口号\n";
                }
            } else if (checkCommand("join")) {
                uint64 lobbyIDVal = 0;
                try {
                    if (!arg.empty()) {
                        lobbyIDVal = std::stoull(arg);
                        if (roomManager.joinLobby(lobbyIDVal)) {
                            std::cout << "正在加入大厅 " << lobbyIDVal << "...\n";
                            monitorMode = true;
                        } else {
                            std::cout << "加入大厅请求失败。\n";
                        }
                    } else {
                        std::cout << "用法：join <大厅ID>\n";
                    }
                } catch (...) {
                    std::cout << "无效大厅ID: " << arg << " (请检查ID是否正确)\n";
                }
            } else if (command == "disconnect") {
                roomManager.leaveLobby();
                steamManager.disconnect();
                if (server) {
                    server->stop();
                    server.reset();
                }
                monitorMode = false;
                std::cout << "已断开连接。\n";
            } else if (command == "friends") {
                std::cout << "好友列表：\n";
                for (const auto& friendPair : SteamUtils::getFriendsList()) {
                    std::cout << " - " << friendPair.second << " (" << friendPair.first.ConvertToUint64() << ")\n";
                }
            } else if (checkCommand("invite")) {
                if (arg.empty()) {
                    std::cout << "用法：invite <名称片段>\n";
                } else {
                    std::string filter = arg;
                    std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
                    bool found = false;
                    for (const auto& friendPair : SteamUtils::getFriendsList()) {
                        std::string name = friendPair.second;
                        std::string nameLower = name;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                        
                        if (nameLower.find(filter) != std::string::npos) {
                            if (SteamMatchmaking()) {
                                SteamMatchmaking()->InviteUserToLobby(roomManager.getCurrentLobby(), friendPair.first);
                                std::cout << "已邀请 " << name << "\n";
                                found = true;
                            }
                        }
                    }
                    if (!found) std::cout << "未找到匹配 '" << filter << "' 的好友\n";
                }
            } else if (command == "status") {
                printStatus(steamManager, roomManager);
            } else if (checkCommand("monitor")) {
                if (arg == "on") monitorMode = true;
                else if (arg == "off") monitorMode = false;
                else std::cout << "用法：monitor [on/off]\n";
            } else {
                std::cout << "未知命令。输入 'help' 查看列表。\n";
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