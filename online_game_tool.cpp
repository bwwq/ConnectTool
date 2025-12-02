#include "steam/steam_networking_manager.h"
#include "steam/steam_room_manager.h"
#include "steam/steam_utils.h"
#include "tcp_server.h"
#include <filesystem>
#include "ui/ui_theme.h"
#include "ui/ui_components.h"
#include "config/config_manager.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <boost/asio.hpp>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#else
#include <signal.h>
#include <sys/file.h>
#include <unistd.h>
#endif

using boost::asio::ip::tcp;

// New variables for multiple connections and TCP clients
std::vector<HSteamNetConnection> connections;
std::mutex connectionsMutex; // Add mutex for connections
int localPort = 0;
std::unique_ptr<TCPServer> server;

#ifdef _WIN32
// Windows implementation using mutex and shared memory
HANDLE g_hMutex = nullptr;
HANDLE g_hMapFile = nullptr;
HWND *g_pSharedHwnd = nullptr;

bool checkSingleInstance() {
  g_hMutex = CreateMutexW(nullptr, FALSE,
                          L"Global\\OnlineGameTool_SingleInstance_Mutex");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    // Another instance exists, try to find and activate it
    g_hMapFile = OpenFileMappingW(FILE_MAP_READ, FALSE,
                                  L"Global\\OnlineGameTool_HWND_Share");
    if (g_hMapFile != nullptr) {
      HWND *pHwnd =
          (HWND *)MapViewOfFile(g_hMapFile, FILE_MAP_READ, 0, 0, sizeof(HWND));
      if (pHwnd != nullptr && *pHwnd != nullptr && IsWindow(*pHwnd)) {
        // Restore and bring to front
        if (IsIconic(*pHwnd)) {
          ShowWindow(*pHwnd, SW_RESTORE);
        }
        SetForegroundWindow(*pHwnd);
        UnmapViewOfFile(pHwnd);
      }
      CloseHandle(g_hMapFile);
    }
    if (g_hMutex) {
      CloseHandle(g_hMutex);
    }
    return false;
  }

  // Create shared memory for HWND
  g_hMapFile =
      CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                         sizeof(HWND), L"Global\\OnlineGameTool_HWND_Share");
  if (g_hMapFile != nullptr) {
    g_pSharedHwnd = (HWND *)MapViewOfFile(g_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0,
                                          sizeof(HWND));
  }
  return true;
}

void storeWindowHandle(GLFWwindow *window) {
  if (g_pSharedHwnd != nullptr) {
    *g_pSharedHwnd = glfwGetWin32Window(window);
  }
}

void cleanupSingleInstance() {
  if (g_pSharedHwnd != nullptr) {
    UnmapViewOfFile(g_pSharedHwnd);
    g_pSharedHwnd = nullptr;
  }
  if (g_hMapFile != nullptr) {
    CloseHandle(g_hMapFile);
    g_hMapFile = nullptr;
  }
  if (g_hMutex != nullptr) {
    CloseHandle(g_hMutex);
    g_hMutex = nullptr;
  }
}

#else
// Unix/Linux/macOS implementation using file lock and signal
int g_lockfd = -1;
std::string g_lockFilePath;

void signalHandler(int signum) {
  // Signal received to bring window to front
  std::cout << "Received signal to activate window" << std::endl;
}

bool checkSingleInstance() {
  std::string tempDir;
#ifdef __APPLE__
  const char *tmpdir = getenv("TMPDIR");
  tempDir = tmpdir ? tmpdir : "/tmp";
#else
  tempDir = "/tmp";
#endif

  g_lockFilePath = tempDir + "/OnlineGameTool.lock";

  g_lockfd = open(g_lockFilePath.c_str(), O_CREAT | O_RDWR, 0666);
  if (g_lockfd < 0) {
    std::cerr << "Failed to open lock file" << std::endl;
    return false;
  }

  // Try to acquire exclusive lock
  if (flock(g_lockfd, LOCK_EX | LOCK_NB) != 0) {
    // Lock failed, another instance is running
    // Read PID and send signal
    char pidBuf[32];
    ssize_t bytesRead = read(g_lockfd, pidBuf, sizeof(pidBuf) - 1);
    if (bytesRead > 0) {
      pidBuf[bytesRead] = '\0';
      pid_t existingPid = atoi(pidBuf);
      if (existingPid > 0) {
        // Send SIGUSR1 to existing instance
        kill(existingPid, SIGUSR1);
      }
    }
    close(g_lockfd);
    g_lockfd = -1;
    return false;
  }

  // Write our PID to the lock file
  ftruncate(g_lockfd, 0);
  pid_t myPid = getpid();
  std::string pidStr = std::to_string(myPid);
  write(g_lockfd, pidStr.c_str(), pidStr.length());

  // Set up signal handler
  signal(SIGUSR1, signalHandler);

  return true;
}

void storeWindowHandle(GLFWwindow *window) {
  // GLFW doesn't provide a standard way to bring window to front on Unix
  // but we can request attention
  glfwRequestWindowAttention(window);
}

void cleanupSingleInstance() {
  if (g_lockfd >= 0) {
    flock(g_lockfd, LOCK_UN);
    close(g_lockfd);
    g_lockfd = -1;
    unlink(g_lockFilePath.c_str());
  }
}
#endif

int main() {
  // Check for single instance
  if (!checkSingleInstance()) {
    std::cout << "另一个实例已在运行，正在激活该窗口..." << std::endl;
    return 0;
  }

  // Initialize Steam API first
  if (!SteamAPI_Init()) {
    std::cerr << "Failed to initialize Steam API" << std::endl;
    return 1;
  }

  boost::asio::io_context io_context;
  auto work_guard = boost::asio::make_work_guard(io_context);
  std::thread io_thread([&io_context]() { io_context.run(); });

  // Initialize Steam Networking Manager
  SteamNetworkingManager steamManager;
  if (!steamManager.initialize()) {
    std::cerr << "Failed to initialize Steam Networking Manager" << std::endl;
    SteamAPI_Shutdown();
    return 1;
  }

  // Initialize Steam Room Manager
  SteamRoomManager roomManager(&steamManager);

  // Initialize GLFW
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW" << std::endl;
    steamManager.shutdown();
    return -1;
  }

#ifdef __APPLE__
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on Mac
#endif

  // Create window
  GLFWwindow *window =
      glfwCreateWindow(1280, 720, "在线游戏工具 - 1.0.0", nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    cleanupSingleInstance();
    SteamAPI_Shutdown();
    return -1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // Enable vsync

  // Store window handle for single instance activation
  storeWindowHandle(window);

  // Initialize Config Manager
  Config::ConfigManager configManager;
  configManager.load();
  const auto& appConfig = configManager.getConfig();

  // Initialize ImGui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  
  // Set font size from config
  float fontSize = appConfig.fontSize;
  
  // Load Chinese font
  // Load Chinese font if exists, otherwise use default
  if (std::filesystem::exists("font.ttf")) {
      io.Fonts->AddFontFromFileTTF(
          "font.ttf", fontSize, nullptr,
          io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
  } else {
      ImFontConfig config;
      config.SizePixels = fontSize;
      io.Fonts->AddFontDefault(&config);
  }
      
  // Apply UI Theme
  UITheme::ApplyTheme();
  
  // Initialize Notification Manager
  UIComponents::NotificationManager notificationManager;

  // Initialize ImGui backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  const char *glsl_version = "#version 130";
#ifdef __APPLE__
  glsl_version = "#version 150";
#endif
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Set message handler dependencies
  steamManager.setMessageHandlerDependencies(io_context, server, localPort);
  steamManager.startMessageHandler();

  // Steam Networking variables
  bool isHost = false;
  bool isClient = false;
  char joinBuffer[256] = "";
  char filterBuffer[256] = "";

  // Lambda to get connection info for a member
  auto getMemberConnectionInfo =
      [&](const CSteamID &memberID,
          const CSteamID &hostSteamID) -> std::pair<int, std::string> {
    int ping = 0;
    std::string relayInfo = "-";

    if (steamManager.isHost()) {
      // Find connection for this member
      std::lock_guard<std::mutex> lockConn(connectionsMutex);
      for (const auto &conn : steamManager.getConnections()) {
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
      // Client only shows ping to host, not to other clients
      if (memberID == hostSteamID) {
        ping = steamManager.getHostPing();
        if (steamManager.getConnection() != k_HSteamNetConnection_Invalid) {
          relayInfo =
              steamManager.getConnectionRelayInfo(steamManager.getConnection());
        }
      }
    }

    return {ping, relayInfo};
  };

  // Lambda to render invite friends UI
  auto renderInviteFriends = [&]() {
    ImGui::InputText("过滤朋友", filterBuffer, IM_ARRAYSIZE(filterBuffer));
    ImGui::Text("朋友:");
    for (const auto &friendPair : SteamUtils::getFriendsList()) {
      std::string nameStr = friendPair.second;
      std::string filterStr(filterBuffer);
      // Convert to lowercase for case-insensitive search
      std::transform(nameStr.begin(), nameStr.end(), nameStr.begin(),
                     ::tolower);
      std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(),
                     ::tolower);
      if (filterStr.empty() || nameStr.find(filterStr) != std::string::npos) {
        ImGui::PushID(static_cast<int>(friendPair.first.ConvertToUint64() & 0xFFFFFFFF));
        if (ImGui::Button(("邀请 " + friendPair.second).c_str())) {
          // Send invite via Steam to lobby
          if (SteamMatchmaking()) {
            SteamMatchmaking()->InviteUserToLobby(roomManager.getCurrentLobby(),
                                                  friendPair.first);
            std::cout << "Sent lobby invite to " << friendPair.second
                      << std::endl;
          } else {
            std::cerr << "SteamMatchmaking() is null! Cannot send invite."
                      << std::endl;
          }
        }
        ImGui::PopID();
      }
    }
  };

  // Frame rate limiting
  const double targetFrameTimeForeground = 1.0 / 60.0; // 60 FPS when focused
  const double targetFrameTimeBackground = 1.0; // 1 FPS when in background
  double lastFrameTime = glfwGetTime();

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    // Frame rate control based on window focus
    bool isFocused = glfwGetWindowAttrib(window, GLFW_FOCUSED);
    double targetFrameTime =
        isFocused ? targetFrameTimeForeground : targetFrameTimeBackground;

    double currentTime = glfwGetTime();
    double deltaTime = currentTime - lastFrameTime;
    if (deltaTime < targetFrameTime) {
      std::this_thread::sleep_for(
          std::chrono::duration<double>(targetFrameTime - deltaTime));
    }
    lastFrameTime = glfwGetTime();

    // Poll events
    glfwPollEvents();

    SteamAPI_RunCallbacks();

    // Update Steam networking info
    steamManager.update();
    
    // Update notifications
    notificationManager.update(static_cast<float>(deltaTime));

    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Set up main window to fill the viewport
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("MainDockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(2);

    // Header
    {
        ImGui::BeginGroup();
        UIComponents::TitleText("ConnectTool");
        ImGui::SameLine();
        if (steamManager.isConnected()) {
             UIComponents::StatusIndicator("已连接", true, steamManager.isHost() ? -1 : steamManager.getHostPing());
        } else {
             UIComponents::StatusIndicator("未连接", false);
        }
        ImGui::EndGroup();
        UIComponents::SeparatorWithText("状态");
    }

    // Content
    if (!steamManager.isHost() && !steamManager.isConnected()) {
        // Not connected view
        
        // Host Card
        if (UIComponents::BeginCard("HostGameCard", ImVec2(0, 120))) {
            UIComponents::TitleText("创建房间");
            UIComponents::HelpText("创建一个新的游戏房间，邀请好友加入。");
            ImGui::Spacing();
            if (UIComponents::BigButton("主持游戏房间")) {
                if (roomManager.startHosting()) {
                    notificationManager.addNotification("房间创建成功！", UIComponents::NotificationType::Success);
                } else {
                    notificationManager.addNotification("创建房间失败", UIComponents::NotificationType::Error);
                }
            }
            UIComponents::EndCard();
        }
        
        ImGui::Spacing();
        
        // Join Card
        if (UIComponents::BeginCard("JoinGameCard", ImVec2(0, 150))) {
            UIComponents::TitleText("加入房间");
            UIComponents::HelpText("输入房间ID加入现有游戏。");
            ImGui::Spacing();
            
            ImGui::InputText("房间ID", joinBuffer, IM_ARRAYSIZE(joinBuffer));
            ImGui::Spacing();
            
            if (UIComponents::BigButton("加入游戏房间")) {
                try {
                    uint64 hostID = std::stoull(joinBuffer);
                    if (steamManager.joinHost(hostID)) {
                        // Start TCP Server
                        server = std::make_unique<TCPServer>(appConfig.tcpServerPort, &steamManager);
                        if (!server->start()) {
                            notificationManager.addNotification("TCP服务器启动失败", UIComponents::NotificationType::Error);
                        } else {
                            notificationManager.addNotification("已加入房间", UIComponents::NotificationType::Success);
                            configManager.addRecentRoom(hostID);
                            configManager.save();
                        }
                    } else {
                        notificationManager.addNotification("加入房间失败", UIComponents::NotificationType::Error);
                    }
                } catch (...) {
                    notificationManager.addNotification("无效的房间ID", UIComponents::NotificationType::Warning);
                }
            }
            UIComponents::EndCard();
        }
        
        // Recent Rooms
        const auto& recentRooms = configManager.getRecentRooms();
        if (!recentRooms.empty()) {
            ImGui::Spacing();
            if (UIComponents::BeginCard("RecentRoomsCard")) {
                UIComponents::TitleText("最近加入");
                ImGui::Spacing();
                for (uint64_t roomID : recentRooms) {
                    std::string idStr = std::to_string(roomID);
                    if (ImGui::Button(("加入 " + idStr).c_str())) {
                         strncpy(joinBuffer, idStr.c_str(), sizeof(joinBuffer) - 1);
                    }
                }
                UIComponents::EndCard();
            }
        }

    } else {
        // Connected view
        
        // Current Room Info
        if (UIComponents::BeginCard("RoomInfoCard", ImVec2(0, 0))) {
            UIComponents::TitleText(steamManager.isHost() ? "正在主持房间" : "已连接到房间");
            ImGui::Spacing();
            
            if (roomManager.getCurrentLobby().IsValid()) {
                std::string lobbyIDStr = std::to_string(roomManager.getCurrentLobby().ConvertToUint64());
                UIComponents::CopyableText("房间ID:", lobbyIDStr.c_str());
            }
            
            if (server) {
                ImGui::Text("TCP服务器: 监听端口 %d", appConfig.tcpServerPort);
                ImGui::Text("客户端数量: %d", server->getClientCount());
            }
            
            ImGui::Spacing();
            if (steamManager.isHost()) {
                 ImGui::InputInt("本地端口", &localPort);
            }
            
            ImGui::Spacing();
            if (UIComponents::DangerButton("断开连接")) {
                roomManager.leaveLobby();
                steamManager.disconnect();
                if (server) {
                    server->stop();
                    server.reset();
                }
                notificationManager.addNotification("已断开连接", UIComponents::NotificationType::Info);
            }
            UIComponents::EndCard();
        }
        
        ImGui::Spacing();
        
        // Members & Invite
        ImGui::Columns(2, "RoomColumns", true);
        
        // Left: Members
        ImGui::BeginChild("MembersList", ImVec2(0, 300));
        UIComponents::TitleText("房间成员");
        ImGui::Spacing();
        
        if (ImGui::BeginTable("UserTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("名称");
            ImGui::TableSetupColumn("延迟");
            ImGui::TableSetupColumn("状态");
            ImGui::TableHeadersRow();
            
            std::vector<CSteamID> members = roomManager.getLobbyMembers();
            CSteamID mySteamID = SteamUser()->GetSteamID();
            CSteamID hostSteamID = steamManager.getHostSteamID();
            
            for (const auto &memberID : members) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", SteamFriends()->GetFriendPersonaName(memberID));
                
                ImGui::TableNextColumn();
                if (memberID == mySteamID) {
                    ImGui::Text("-");
                } else {
                    auto [ping, relayInfo] = getMemberConnectionInfo(memberID, hostSteamID);
                    ImGui::Text("%d ms", ping);
                }
                
                ImGui::TableNextColumn();
                if (memberID == hostSteamID) ImGui::Text("房主");
                else ImGui::Text("成员");
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
        
        ImGui::NextColumn();
        
        // Right: Invite
        ImGui::BeginChild("InviteList", ImVec2(0, 300));
        UIComponents::TitleText("邀请好友");
        ImGui::InputText("搜索", filterBuffer, IM_ARRAYSIZE(filterBuffer));
        ImGui::Spacing();
        
        ImGui::BeginChild("FriendsScroll");
        for (const auto &friendPair : SteamUtils::getFriendsList()) {
            std::string nameStr = friendPair.second;
            std::string filterStr(filterBuffer);
            std::transform(nameStr.begin(), nameStr.end(), nameStr.begin(), ::tolower);
            std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);
            
            if (filterStr.empty() || nameStr.find(filterStr) != std::string::npos) {
                ImGui::PushID(static_cast<int>(friendPair.first.ConvertToUint64() & 0xFFFFFFFF));
                if (ImGui::Button(("邀请 " + friendPair.second).c_str())) {
                    if (SteamMatchmaking()) {
                        SteamMatchmaking()->InviteUserToLobby(roomManager.getCurrentLobby(), friendPair.first);
                        notificationManager.addNotification("已发送邀请给 " + friendPair.second, UIComponents::NotificationType::Success);
                    }
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::EndChild();
        
        ImGui::Columns(1);
    }

    ImGui::End();
    
    // Render notifications overlay
    notificationManager.render();

    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    ImVec4 clear_color = UITheme::Colors::Background;
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Swap buffers
    glfwSwapBuffers(window);
  }

  // Stop message handler
  steamManager.stopMessageHandler();

  // Cleanup
  if (server) {
    server->stop();
  }

  // Stop io_context and join thread
  work_guard.reset();
  io_context.stop();
  if (io_thread.joinable()) {
    io_thread.join();
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  steamManager.shutdown();

  // Cleanup single instance resources
  cleanupSingleInstance();

  return 0;
}