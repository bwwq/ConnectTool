#include "multiplex_manager.h"
#include "nanoid/nanoid.h"
#include <iostream>
#include <cstring>

MultiplexManager::MultiplexManager(ISteamNetworkingSockets *steamInterface, HSteamNetConnection steamConn,
                                   boost::asio::io_context &io_context, bool &isHost, int &localPort)
    : steamInterface_(steamInterface), steamConn_(steamConn),
      io_context_(io_context), isHost_(isHost), localPort_(localPort) {}

MultiplexManager::~MultiplexManager()
{
    // Close all sockets
    std::lock_guard<std::mutex> lock(mapMutex_);
    for (auto &pair : clientMap_)
    {
        pair.second->close();
    }
    clientMap_.clear();
}

std::string MultiplexManager::addClient(std::shared_ptr<tcp::socket> socket)
{
    std::string id;
    {
        std::lock_guard<std::mutex> lock(mapMutex_);
        id = nanoid::generate(6);
        clientMap_[id] = socket;
        readBuffers_[id].resize(131072); // 128KB
    }
    startAsyncRead(id);
    std::cout << "Added client with id " << id << std::endl;
    return id;
}

void MultiplexManager::removeClient(const std::string &id)
{
    std::lock_guard<std::mutex> lock(mapMutex_);
    auto it = clientMap_.find(id);
    if (it != clientMap_.end())
    {
        it->second->close();
        clientMap_.erase(it);
    }
    readBuffers_.erase(id);

    std::cout << "Removed client with id " << id << std::endl;
}

std::shared_ptr<tcp::socket> MultiplexManager::getClient(const std::string &id)
{
    std::lock_guard<std::mutex> lock(mapMutex_);
    auto it = clientMap_.find(id);
    if (it != clientMap_.end())
    {
        return it->second;
    }
    return nullptr;
}

void MultiplexManager::sendTunnelPacket(const std::string &id, const char *data, size_t len, int type)
{
    // Packet format: string id (6 chars + null), uint32_t type, then data if type==0
    size_t idLen = id.size() + 1; // include null terminator
    size_t packetSize = idLen + sizeof(uint32_t) + (type == 0 ? len : 0);
    std::vector<char> packet(packetSize);
    std::memcpy(&packet[0], id.c_str(), idLen);
    uint32_t *pType = reinterpret_cast<uint32_t *>(&packet[idLen]);
    *pType = type;
    if (type == 0 && data)
    {
        std::memcpy(&packet[idLen + sizeof(uint32_t)], data, len);
    }
    steamInterface_->SendMessageToConnection(steamConn_, packet.data(), packet.size(), k_nSteamNetworkingSend_Reliable, nullptr);
}

void MultiplexManager::handleTunnelPacket(const char *data, size_t len)
{
    size_t idLen = 7; // 6 + null
    if (len < idLen + sizeof(uint32_t))
    {
        std::cerr << "Invalid tunnel packet size" << std::endl;
        return;
    }
    std::string id(data, 6);
    uint32_t type = *reinterpret_cast<const uint32_t *>(data + idLen);
    if (type == 0)
    {
        // Data packet
        size_t dataLen = len - idLen - sizeof(uint32_t);
        const char *packetData = data + idLen + sizeof(uint32_t);
        auto socket = getClient(id);
        if (!socket && isHost_ && localPort_ > 0)
        {
            // 如果是主持且没有对应的 TCP Client，创建一个连接到本地端口
            std::cout << "Creating new TCP client for id " << id << " connecting to localhost:" << localPort_ << std::endl;
            try
            {
                auto newSocket = std::make_shared<tcp::socket>(io_context_);
                tcp::resolver resolver(io_context_);
                auto endpoints = resolver.resolve("127.0.0.1", std::to_string(localPort_));
                boost::asio::connect(*newSocket, endpoints);
                newSocket->set_option(tcp::no_delay(true)); // Enable TCP NoDelay

                std::string tempId = id;
                {
                    std::lock_guard<std::mutex> lock(mapMutex_);
                    clientMap_[id] = newSocket;
                    readBuffers_[id].resize(131072); // 128KB
                    socket = newSocket;
                }
                std::cout << "Successfully created TCP client for id " << id << std::endl;
                startAsyncRead(tempId);
            }
            catch (const std::exception &e)
            {
                std::cerr << "Failed to create TCP client for id " << id << ": " << e.what() << std::endl;
                return;
            }
        }
        if (socket)
        {
            // CRITICAL FIX: Copy data to a shared buffer to keep it alive for async_write
            // The original 'packetData' is freed immediately after this function returns!
            auto buffer = std::make_shared<std::vector<char>>(packetData, packetData + dataLen);
            boost::asio::async_write(*socket, boost::asio::buffer(*buffer), 
                [buffer](const boost::system::error_code &, std::size_t) {});
        }
        else
        {
            std::cerr << "No client found for id " << id << std::endl;
        }
    }
    else if (type == 1)
    {
        // Disconnect packet
        removeClient(id);
        std::cout << "Client " << id << " disconnected" << std::endl;
    }
    else if (type == 2) // Ping
    {
        // Send Pong
        sendTunnelPacket(id, data + idLen + sizeof(uint32_t), len - idLen - sizeof(uint32_t), 3);
    }
    else if (type == 3) // Pong
    {
        auto now = std::chrono::steady_clock::now();
        auto sentTime = *reinterpret_cast<const std::chrono::steady_clock::time_point*>(data + idLen + sizeof(uint32_t));
        auto rtt = std::chrono::duration_cast<std::chrono::milliseconds>(now - sentTime).count();
        // std::cout << "[Ping] Pong received! RTT: " << rtt << " ms" << std::endl; // Silenced for performance
        std::cout << "RTT: " << rtt << " ms\r" << std::flush; // Print RTT in-place
    }
    else
    {
        std::cerr << "Unknown packet type " << type << std::endl;
    }
}

void MultiplexManager::sendPing()
{
    std::string id = "PING"; // Dummy ID
    auto now = std::chrono::steady_clock::now();
    sendTunnelPacket(id, reinterpret_cast<const char*>(&now), sizeof(now), 2);
    // std::cout << "[Ping] Sending Ping..." << std::endl; // Silenced
}

void MultiplexManager::startAsyncRead(const std::string &id)
{
    std::shared_ptr<tcp::socket> socket;
    std::vector<char>* bufferPtr = nullptr;
    {
        std::lock_guard<std::mutex> lock(mapMutex_);
        auto it = clientMap_.find(id);
        if (it == clientMap_.end()) {
            return; // Client already removed
        }
        socket = it->second;
        bufferPtr = &readBuffers_[id];
    }
    
    if (!socket || !socket->is_open()) {
        return;
    }
    
    // Capture socket by value (shared_ptr) to keep it alive during async operation
    socket->async_read_some(boost::asio::buffer(*bufferPtr),
    [this, id, socket, bufferPtr](const boost::system::error_code &ec, std::size_t bytes_transferred)
    {
        if (!ec)
        {
            if (bytes_transferred > 0)
            {
                // Check if client still exists before sending
                bool clientExists = false;
                {
                    std::lock_guard<std::mutex> lock(mapMutex_);
                    clientExists = (clientMap_.find(id) != clientMap_.end());
                }
                if (clientExists) {
                    sendTunnelPacket(id, bufferPtr->data(), bytes_transferred, 0);
                }
            }
            startAsyncRead(id);
        }
        else
        {
            if (ec != boost::asio::error::operation_aborted) {
                std::cout << "Error reading from TCP client " << id << ": " << ec.message() << std::endl;
            }
            removeClient(id);
        }
    });
}