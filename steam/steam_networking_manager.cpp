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

    // 1. 允许 P2P (ICE) 直连 - 重新启用以支持 VPS/复杂网络环境
    int32 nIceEnable = k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_Public | k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_Private;
    SteamNetworkingUtils()->SetConfigValue(
        k_ESteamNetworkingConfig_P2P_Transport_ICE_Enable,
        k_ESteamNetworkingConfig_Global,
        0,
        k_ESteamNetworkingConfig_Int32,
        &nIceEnable);

    // 2. 优化对称 NAT 连接 (对 VPS/云服务器很重要)
    int32 symmetricConnect = 1;
    SteamNetworkingUtils()->SetConfigValue(
        k_ESteamNetworkingConfig_SymmetricConnect,
        k_ESteamNetworkingConfig_Global,
        0,
        k_ESteamNetworkingConfig_Int32,
        &symmetricConnect);

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

    // ============ Performance Optimization (Reference: ConnectTool-tun) ============
    
    // 1. Disable Nagle's algorithm for lower latency
    int32 nagleTime = 0;
    SteamNetworkingUtils()->SetConfigValue(
        k_ESteamNetworkingConfig_NagleTime,
        k_ESteamNetworkingConfig_Global,
        0,
        k_ESteamNetworkingConfig_Int32,
        &nagleTime);

    // 2. Increase Send Rate (5MB/s) to handle large Minecraft chunks
    int32 sendRate = 5 * 1024 * 1024; 
    SteamNetworkingUtils()->SetConfigValue(
        k_ESteamNetworkingConfig_SendRateMin,
        k_ESteamNetworkingConfig_Global,
        0,
        k_ESteamNetworkingConfig_Int32,
        &sendRate);
    SteamNetworkingUtils()->SetConfigValue(
        k_ESteamNetworkingConfig_SendRateMax,
        k_ESteamNetworkingConfig_Global,
        0,
        k_ESteamNetworkingConfig_Int32,
        &sendRate);

    // 3. Increase Send Buffer (10MB)
    int32 sendBufferSize = 10 * 1024 * 1024;
    SteamNetworkingUtils()->SetConfigValue(
        k_ESteamNetworkingConfig_SendBufferSize,
        k_ESteamNetworkingConfig_Global,
        0,
        k_ESteamNetworkingConfig_Int32,
        &sendBufferSize);

    // 4. Optimize MTU (Safe default for UDP tunneling)
    int32 mtu = 1200;
    SteamNetworkingUtils()->SetConfigValue(
        k_ESteamNetworkingConfig_MTU_PacketSize,
        k_ESteamNetworkingConfig_Global,
        0,
        k_ESteamNetworkingConfig_Int32,
        &mtu);

    std::cout << "[配置] 已应用高性能网络参数 (NoDelay, 5MB/s Rate, 10MB Buffer)" << std::endl;

    // Create callbacks after Steam API init
    SteamNetworkingUtils()->InitRelayNetworkAccess();
    SteamNetworkingUtils()->SetGlobalCallback_SteamNetConnectionStatusChanged(OnSteamNetConnectionStatusChanged);

    m_pInterface = SteamNetworkingSockets();

    // Check if callbacks are registered
    std::cout << "Steam Networking Manager initialized successfully" << std::endl;
    
    CSteamID localID = SteamUser()->GetSteamID();
    std::cout << "[Steam] 当前登录用户ID: " << localID.ConvertToUint64() << std::endl;

    return true;
}

void SteamNetworkingManager::setForceRelay(bool force)
{
    int32 iceEnable = force ? k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_Disable : (k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_Public | k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_Private);
    SteamNetworkingUtils()->SetConfigValue(
        k_ESteamNetworkingConfig_P2P_Transport_ICE_Enable,
        k_ESteamNetworkingConfig_Global,
        0,
        k_ESteamNetworkingConfig_Int32,
        &iceEnable);
        
    std::cout << (force ? "[配置] 已开启强制中继模式 (Force Relay)。" : "[配置] 已关闭强制中继模式 (Auto P2P)。") << std::endl;
}

void SteamNetworkingManager::printRelayStatus()
{
    SteamRelayNetworkStatus_t status;
    SteamNetworkingUtils()->GetRelayNetworkStatus(&status);
    
    std::cout << "=== Steam Relay Network Status ===" << std::endl;
    std::cout << "Availability: ";
    switch (status.m_eAvail) {
        case k_ESteamNetworkingAvailability_CannotTry: std::cout << "CannotTry (Critical Error)"; break;
        case k_ESteamNetworkingAvailability_Failed: std::cout << "Failed (Check Internet/Firewall)"; break;
        case k_ESteamNetworkingAvailability_Previously: std::cout << "Previously Available (Retrying...)"; break;
        case k_ESteamNetworkingAvailability_Retrying: std::cout << "Retrying..."; break;
        case k_ESteamNetworkingAvailability_NeverTried: std::cout << "NeverTried (Wait a bit)"; break;
        case k_ESteamNetworkingAvailability_Waiting: std::cout << "Waiting for Config..."; break;
        case k_ESteamNetworkingAvailability_Attempting: std::cout << "Attempting Connection..."; break;
        case k_ESteamNetworkingAvailability_Current: std::cout << "Current (OK)"; break;
        case k_ESteamNetworkingAvailability_Unknown: std::cout << "Unknown"; break;
        default: std::cout << "Code " << status.m_eAvail; break;
    }
    std::cout << std::endl;
    
    if (status.m_eAvail != k_ESteamNetworkingAvailability_Current) {
        // std::cout << "Debug Msg: " << status.m_szDebugMsg << std::endl; // Not available in this SDK version
        std::cout << "[提示] 如果状态不是 'Current (OK)'，请等待几分钟或检查网络。" << std::endl;
    } else {
        // std::cout << "Ping to Relay: " << status.m_nPingMeasurementFileBytes << " bytes config" << std::endl; // Not available
        std::cout << "Relay Network Configured." << std::endl;
    }
}

void SteamNetworkingManager::sendPing()
{
    if (g_isConnected && g_hConnection != k_HSteamNetConnection_Invalid) {
        auto mm = messageHandler_->getMultiplexManager(g_hConnection);
        if (mm) {
            mm->sendPing();
        }
    } else {
        std::cout << "[Ping] 未连接到主机，无法发送 Ping。" << std::endl;
    }
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
    
    if (hostSteamID == SteamUser()->GetSteamID()) {
        std::cerr << "[错误] 不能连接到自己！请确保您和主机使用不同的 Steam 账号。" << std::endl;
        return false;
    }

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
            } else if (pInfo->m_info.m_eEndReason == 4003) {
                 ss << "\n[提示] 证书验证失败。请务必检查：\n1. 双方电脑/VPS的【系统时间】是否准确（精确到分钟）。\n2. 尝试重启 Steam 客户端以更新证书。";
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