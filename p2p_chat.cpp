#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <steam_api.h>
#include <isteamnetworkingsockets.h>
#include <isteamnetworkingutils.h>
#include <steamnetworkingtypes.h>

// Global variables for callbacks
HSteamNetConnection g_hConnection = k_HSteamNetConnection_Invalid;
bool g_isConnected = false;

// Callback function for connection status changes
void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo)
{
    std::cout << "Connection status changed: " << pInfo->m_info.m_eState << std::endl;
    if (pInfo->m_eOldState == k_ESteamNetworkingConnectionState_None && pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_Connecting)
    {
        // Incoming connection, accept it
        SteamNetworkingSockets()->AcceptConnection(pInfo->m_hConn);
        g_hConnection = pInfo->m_hConn;
        g_isConnected = true;
        std::cout << "Accepted incoming connection" << std::endl;
    }
    else if (pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connecting && pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_Connected)
    {
        // Client connected successfully
        g_isConnected = true;
        std::cout << "Connected to host" << std::endl;
    }
    else if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer || pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)
    {
        // Connection closed
        g_isConnected = false;
        g_hConnection = k_HSteamNetConnection_Invalid;
        std::cout << "Connection closed" << std::endl;
    }
}

int main() {
    // Initialize Steam API
    if (!SteamAPI_Init()) {
        std::cerr << "Failed to initialize Steam API" << std::endl;
        return 1;
    }

    // Initialize Steam Networking Sockets
    SteamNetworkingUtils()->InitRelayNetworkAccess();

    // Set global callback for connection status changes
    SteamNetworkingUtils()->SetGlobalCallback_SteamNetConnectionStatusChanged(OnSteamNetConnectionStatusChanged);

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        SteamAPI_Shutdown();
        return -1;
    }

    // Create window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Steam P2P Chat", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        SteamAPI_Shutdown();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    // Load Chinese font if available
    io.Fonts->AddFontFromFileTTF("font.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    ImGui::StyleColorsDark();

    // Initialize ImGui backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Steam Networking variables
    HSteamListenSocket hListenSock = k_HSteamListenSocket_Invalid;
    ISteamNetworkingSockets* m_pInterface = SteamNetworkingSockets();

    // Chat variables
    std::vector<std::string> messages;
    char inputBuffer[256] = "";
    CSteamID selectedFriend;
    bool isHost = false;
    bool isClient = false;
    char filterBuffer[256] = "";

    // Get friends list
    std::vector<CSteamID> friendsList;
    int friendCount = SteamFriends()->GetFriendCount(k_EFriendFlagAll);
    for (int i = 0; i < friendCount; ++i) {
        CSteamID friendID = SteamFriends()->GetFriendByIndex(i, k_EFriendFlagAll);
        friendsList.push_back(friendID);
    }

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Poll events
        glfwPollEvents();

        // Run Steam callbacks
        SteamAPI_RunCallbacks();

        // Poll networking
        m_pInterface->RunCallbacks();

        // Receive messages
        if (g_isConnected) {
            ISteamNetworkingMessage* pIncomingMsg = nullptr;
            int numMsgs = m_pInterface->ReceiveMessagesOnConnection(g_hConnection, &pIncomingMsg, 1);
            if (numMsgs > 0 && pIncomingMsg) {
                std::string msg((char*)pIncomingMsg->m_pData, pIncomingMsg->m_cbSize);
                messages.push_back("Friend: " + msg);
                pIncomingMsg->Release();
            }
        }

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Main menu
        ImGui::Begin("Steam P2P Chat");
        if (!isHost && !g_isConnected) {
            if (ImGui::Button("Host Chat Room")) {
                // Create listen socket
                hListenSock = m_pInterface->CreateListenSocketP2P(0, 0, nullptr);
                if (hListenSock != k_HSteamListenSocket_Invalid) {
                    isHost = true;
                    // Set Rich Presence
                    std::string connectStr = std::to_string(SteamUser()->GetSteamID().ConvertToUint64());
                    SteamFriends()->SetRichPresence("connect", connectStr.c_str());
                    SteamFriends()->SetRichPresence("status", "Hosting Chat Room");
                    std::cout << "Hosting chat room. Connect string: " << connectStr << std::endl;
                }
            }
            static char joinBuffer[256] = "";
            ImGui::InputText("Host Steam ID", joinBuffer, IM_ARRAYSIZE(joinBuffer));
            if (ImGui::Button("Join Chat Room")) {
                uint64 hostID = std::stoull(joinBuffer);
                CSteamID hostSteamID(hostID);
                isClient = true;
                // Connect to host
                SteamNetworkingIdentity identity;
                identity.SetSteamID(hostSteamID);
                g_hConnection = m_pInterface->ConnectP2P(identity, 0, 0, nullptr);
                if (g_hConnection != k_HSteamNetConnection_Invalid) {
                    // Connection initiated, wait for callback to confirm
                    std::cout << "Connecting to host..." << std::endl;
                }
            }
        }
        if (isHost) {
            ImGui::Text("Hosting chat room. Invite friends!");
            ImGui::Separator();
            ImGui::InputText("Filter Friends", filterBuffer, IM_ARRAYSIZE(filterBuffer));
            ImGui::Text("Friends:");
            for (size_t i = 0; i < friendsList.size(); ++i) {
                const char* name = SteamFriends()->GetFriendPersonaName(friendsList[i]);
                std::string nameStr(name);
                std::string filterStr(filterBuffer);
                // Convert to lowercase for case-insensitive search
                std::transform(nameStr.begin(), nameStr.end(), nameStr.begin(), ::tolower);
                std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);
                if (filterStr.empty() || nameStr.find(filterStr) != std::string::npos) {
                    if (ImGui::Button((std::string("Invite ") + name).c_str())) {
                        SteamFriends()->InviteUserToGame(friendsList[i], "");
                    }
                }
            }
        }
        ImGui::End();

        // Chat window
        if (g_isConnected) {
            ImGui::Begin("Chat Room");
            ImGui::Text("Chatting");

            // Display messages
            ImGui::BeginChild("Messages", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 30), true);
            for (const auto& msg : messages) {
                ImGui::TextWrapped("%s", msg.c_str());
            }
            ImGui::EndChild();

            // Input
            if (ImGui::InputText("Message", inputBuffer, IM_ARRAYSIZE(inputBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (strlen(inputBuffer) > 0) {
                    uint32 msgSize = static_cast<uint32>(strlen(inputBuffer) + 1);
                    m_pInterface->SendMessageToConnection(g_hConnection, inputBuffer, msgSize, k_nSteamNetworkingSend_Reliable, nullptr);
                    messages.push_back("You: " + std::string(inputBuffer));
                    memset(inputBuffer, 0, sizeof(inputBuffer));
                }
            }
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers
        glfwSwapBuffers(window);
    }

    // Cleanup
    if (g_hConnection != k_HSteamNetConnection_Invalid) {
        m_pInterface->CloseConnection(g_hConnection, 0, nullptr, false);
    }
    if (hListenSock != k_HSteamListenSocket_Invalid) {
        m_pInterface->CloseListenSocket(hListenSock);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    SteamAPI_Shutdown();

    return 0;
}