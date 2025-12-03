#include "steam_networking_manager.h"
#include <iostream>
#include <algorithm>
#include <sstream>

SteamNetworkingManager *SteamNetworkingManager::instance = nullptr;

// Static callback function
void SteamNetworkingManager::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo)
{
    if (instance)
    {
        instance->handleConnectionStatusChanged(pInfo);
    }
}

SteamNetworkingManager::SteamNetworkingManager()
    : m_pInterface(nullptr), hListenSock(k_HSteamListenSocket_Invalid), g_isHost(false), g_isClient(false), g_isConnected(false),
      g_hConnection(k_HSteamNetConnection_Invalid),
      io_context_(nullptr), server_(nullptr), localPort_(nullptr), messageHandler_(nullptr), hostPing_(0)
{
}

SteamNetworkingManager::~SteamNetworkingManager()
{
    stopMessageHandler();
    delete messageHandler_;
    shutdown();
}

bool SteamNetworkingManager::initialize()
{
    instance = this;
    
    // Steam API should already be initialized before calling this
    if (!SteamAPI_IsSteamRunning())
    {
        std::cerr << "Steam is not running" << std::endl;
        return false;
    }

    // Disable debug output to prevent interference with CLI UI
    SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_None, nullptr);

    int32 logLevel = k_ESteamNetworkingSocketsDebugOutputType_None;
    SteamNetworkingUtils()->SetConfigValue(
        k_ESteamNetworkingConfig_LogLevel_P2PRendezvous,
        k_ESteamNetworkingConfig_Global,
        0,
        k_ESteamNetworkingConfig_Int32,
        &logLevel);

    // 1. 允许 P2P (ICE) 直连 - 使用默认设置，移除手动配置以避免兼容性问题
    // SteamNetworkingUtils()->SetConfigValue(k_ESteamNetworkingConfig_P2P_Transport_ICE_Enable, ...);

    // 2. 增加连接超时时间，提高稳定性
    int32 timeoutMs = 30000; // 30秒
    SteamNetworkingUtils()->SetConfigValue(
        k_ESteamNetworkingConfig_TimeoutInitial,
        k_ESteamNetworkingConfig_Global,
        0,
        k_ESteamNetworkingConfig_Int32,
        &timeoutMs);
    
    SteamNetworkingUtils()->SetConfigValue(
        k_ESteamNetworkingConfig_TimeoutConnected,
        k_ESteamNetworkingConfig_Global,
        0,
        k_ESteamNetworkingConfig_Int32,
        &timeoutMs);

    // Allow connections from IPs without authentication
    int32 allowWithoutAuth = 2;
    SteamNetworkingUtils()->SetConfigValue(
        k_ESteamNetworkingConfig_IP_AllowWithoutAuth,
        k_ESteamNetworkingConfig_Global,
        0,
        k_ESteamNetworkingConfig_Int32,
        &allowWithoutAuth);

    // Create callbacks after Steam API init
    SteamNetworkingUtils()->InitRelayNetworkAccess();
    SteamNetworkingUtils()->SetGlobalCallback_SteamNetConnectionStatusChanged(OnSteamNetConnectionStatusChanged);

    m_pInterface = SteamNetworkingSockets();

    // Check if callbacks are registered
    std::cout << "Steam Networking Manager initialized successfully" << std::endl;

    return true;
}

void SteamNetworkingManager::shutdown()
{
    if (g_hConnection != k_HSteamNetConnection_Invalid)
    {
        m_pInterface->CloseConnection(g_hConnection, 0, nullptr, false);
    }
    if (hListenSock != k_HSteamListenSocket_Invalid)
    {
        m_pInterface->CloseListenSocket(hListenSock);
    }
    SteamAPI_Shutdown();
}

bool SteamNetworkingManager::joinHost(uint64 hostID)
{
    CSteamID hostSteamID(hostID);
    g_isClient = true;
    g_hostSteamID = hostSteamID;
    SteamNetworkingIdentity identity;
    identity.SetSteamID(hostSteamID);

    g_hConnection = m_pInterface->ConnectP2P(identity, 0, 0, nullptr);

    if (g_hConnection != k_HSteamNetConnection_Invalid)
    {
        std::cout << "[客户端] 正在连接主机 " << hostSteamID.ConvertToUint64() << "...\033[K\n";
        return true;
    }
    else
    {
        std::cerr << "Failed to initiate connection" << std::endl;
        return false;
    }
}

void SteamNetworkingManager::disconnect()
{
    std::lock_guard<std::mutex> lock(connectionsMutex);
    
    // Close client connection
    if (g_hConnection != k_HSteamNetConnection_Invalid)
    {
        m_pInterface->CloseConnection(g_hConnection, 0, nullptr, false);
        g_hConnection = k_HSteamNetConnection_Invalid;
    }
    
    // Close all host connections
    for (auto conn : connections)
    {
        m_pInterface->CloseConnection(conn, 0, nullptr, false);
    }
    connections.clear();
    
    // Close listen socket
    if (hListenSock != k_HSteamListenSocket_Invalid)
    {
        m_pInterface->CloseListenSocket(hListenSock);
        hListenSock = k_HSteamListenSocket_Invalid;
    }
    
    // Reset state
    g_isHost = false;
    g_isClient = false;
    g_isConnected = false;
    hostPing_ = 0;
    
    std::cout << "Disconnected from network" << std::endl;
}

void SteamNetworkingManager::setMessageHandlerDependencies(boost::asio::io_context &io_context, std::unique_ptr<TCPServer> &server, int &localPort)
{
    io_context_ = &io_context;
    server_ = &server;
    localPort_ = &localPort;
    messageHandler_ = new SteamMessageHandler(io_context, m_pInterface, connections, connectionsMutex, g_isHost, localPort);
}

void SteamNetworkingManager::startMessageHandler()
{
    if (messageHandler_)
    {
        messageHandler_->start();
    }
}

void SteamNetworkingManager::stopMessageHandler()
{
    if (messageHandler_)
    {
        messageHandler_->stop();
    }
}

void SteamNetworkingManager::update()
{
    std::lock_guard<std::mutex> lock(connectionsMutex);
    // Update ping to host/client connection
    if (g_hConnection != k_HSteamNetConnection_Invalid)
    {
        SteamNetConnectionRealTimeStatus_t status;
        if (m_pInterface->GetConnectionRealTimeStatus(g_hConnection, &status, 0, nullptr))
        {
            hostPing_ = status.m_nPing;
        }
    }
}

int SteamNetworkingManager::getConnectionPing(HSteamNetConnection conn) const
{
    SteamNetConnectionRealTimeStatus_t status;
    if (m_pInterface->GetConnectionRealTimeStatus(conn, &status, 0, nullptr))
    {
        return status.m_nPing;
    }
    return 0;
}

std::string SteamNetworkingManager::getConnectionRelayInfo(HSteamNetConnection conn) const
{
    SteamNetConnectionInfo_t info;
    if (m_pInterface->GetConnectionInfo(conn, &info))
    {
        // Check if connection is using relay
        if (info.m_nFlags & k_nSteamNetworkConnectionInfoFlags_Relayed)
        {
            return "中继";
        }
        else
        {
            return "直连";
        }
    }
    return "N/A";
}

void SteamNetworkingManager::handleConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo)
{
    std::lock_guard<std::mutex> lock(connectionsMutex);
    // Log removed to prevent CLI interference
    
    if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)
    {
        // Connection failed
        std::stringstream ss;
        ss << "连接失败: " << pInfo->m_info.m_szEndDebug 
           << " [代码: " << pInfo->m_info.m_eEndReason << "]"
           << " [描述: " << pInfo->m_info.m_szConnectionDescription << "]";
        m_lastError = ss.str();
    }
    if (pInfo->m_eOldState == k_ESteamNetworkingConnectionState_None && pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_Connecting)
    {
        std::cout << "[主机] 收到连接请求: " << pInfo->m_info.m_identityRemote.GetSteamID().ConvertToUint64() << "\033[K\n";
        m_lastError.clear(); // Clear error on new connection attempt
        m_pInterface->AcceptConnection(pInfo->m_hConn);
        connections.push_back(pInfo->m_hConn);
        g_hConnection = pInfo->m_hConn;
        g_isConnected = true;
    }
    else if (pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connecting && pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_Connected)
    {
        std::cout << "[状态] 连接建立成功！\033[K\n";
        g_isConnected = true;
        m_lastError.clear(); // Clear error on successful connection
        SteamNetConnectionInfo_t info;
        SteamNetConnectionRealTimeStatus_t status;
        if (m_pInterface->GetConnectionInfo(pInfo->m_hConn, &info) && m_pInterface->GetConnectionRealTimeStatus(pInfo->m_hConn, &status, 0, nullptr))
        {
            hostPing_ = status.m_nPing;
        }
    }
    else if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer || pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)
    {
        g_isConnected = false;
        g_hConnection = k_HSteamNetConnection_Invalid;
        
        std::stringstream ss;
        if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer) {
             ss << "连接断开 (对方关闭): " << pInfo->m_info.m_szEndDebug;
        } else {
             if (m_lastError.empty()) {
                 ss << "连接断开 (本地问题): " << pInfo->m_info.m_szEndDebug;
             } else {
                 ss << "连接断开 (本地问题): " << pInfo->m_info.m_szEndDebug;
             }
        }
        
        if (ss.tellp() > 0) {
            ss << " [代码: " << pInfo->m_info.m_eEndReason << "]"
               << " [描述: " << pInfo->m_info.m_szConnectionDescription << "]";
            
            if (pInfo->m_info.m_eEndReason == 5008 || pInfo->m_info.m_eEndReason == 5002 || pInfo->m_info.m_eEndReason == 5003) {
                 ss << "\n[提示] 请检查主机是否已启动 'host' 模式，且双方防火墙允许此程序通行。";
            }
            
            m_lastError = ss.str();
        }

        // Remove from connections
        auto it = std::find(connections.begin(), connections.end(), pInfo->m_hConn);
        if (it != connections.end())
        {
            connections.erase(it);
        }
        hostPing_ = 0;
    }
}