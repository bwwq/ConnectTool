#pragma once
#include <string>
#include <fstream>
#include <vector>
#include <cstdint>

namespace Config {

// 配置数据结构
struct AppConfig {
    // 窗口设置
    int windowWidth = 1280;
    int windowHeight = 720;
    int windowPosX = -1;  // -1 表示居中
    int windowPosY = -1;
    
    // 网络设置
    int tcpServerPort = 8888;
    
    // 房间历史
    std::vector<uint64_t> recentRoomIDs;
    int maxRecentRooms = 5;
    
    // UI 设置
    float fontSize = 18.0f;
    bool showNotifications = true;
};

// 配置管理器
class ConfigManager {
private:
    AppConfig config;
    std::string configFilePath;
    
    // 简单的键值对解析
    void parseLine(const std::string& line);
    std::string trim(const std::string& str);
    
public:
    ConfigManager();
    ~ConfigManager();
    
    // 加载和保存
    bool load();
    bool save();
    
    // 获取和设置
    AppConfig& getConfig() { return config; }
    const AppConfig& getConfig() const { return config; }
    
    // 窗口相关
    void setWindowSize(int width, int height);
    void setWindowPosition(int x, int y);
    
    // 房间历史
    void addRecentRoom(uint64_t roomID);
    const std::vector<uint64_t>& getRecentRooms() const { return config.recentRoomIDs; }
    void clearRecentRooms();
};

} // namespace Config
