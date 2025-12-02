#include "config_manager.h"
#include <iostream>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#endif

namespace Config {

ConfigManager::ConfigManager() {
    // 确定配置文件路径
    #ifdef _WIN32
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        configFilePath = std::string(appDataPath) + "\\ConnectTool\\config.ini";
        
        // 创建目录
        std::string dirPath = std::string(appDataPath) + "\\ConnectTool";
        CreateDirectoryA(dirPath.c_str(), NULL);
    } else {
        configFilePath = "config.ini";
    }
    #else
    const char* homeDir = getenv("HOME");
    if (!homeDir) {
        struct passwd* pw = getpwuid(getuid());
        homeDir = pw->pw_dir;
    }
    configFilePath = std::string(homeDir) + "/.config/ConnectTool/config.ini";
    
    // 创建目录
    std::string dirPath = std::string(homeDir) + "/.config/ConnectTool";
    mkdir(dirPath.c_str(), 0755);
    #endif
}

ConfigManager::~ConfigManager() {
    save();
}

std::string ConfigManager::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

void ConfigManager::parseLine(const std::string& line) {
    size_t pos = line.find('=');
    if (pos == std::string::npos) return;
    
    std::string key = trim(line.substr(0, pos));
    std::string value = trim(line.substr(pos + 1));
    
    if (key == "window_width") {
        config.windowWidth = std::stoi(value);
    } else if (key == "window_height") {
        config.windowHeight = std::stoi(value);
    } else if (key == "window_pos_x") {
        config.windowPosX = std::stoi(value);
    } else if (key == "window_pos_y") {
        config.windowPosY = std::stoi(value);
    } else if (key == "tcp_server_port") {
        config.tcpServerPort = std::stoi(value);
    } else if (key == "font_size") {
        config.fontSize = std::stof(value);
    } else if (key == "show_notifications") {
        config.showNotifications = (value == "1" || value == "true");
    } else if (key == "recent_room") {
        try {
            uint64_t roomID = std::stoull(value);
            config.recentRoomIDs.push_back(roomID);
        } catch (...) {
            // 忽略解析错误
        }
    }
}

bool ConfigManager::load() {
    std::ifstream file(configFilePath);
    if (!file.is_open()) {
        std::cout << "配置文件不存在，将使用默认配置: " << configFilePath << std::endl;
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        parseLine(line);
    }
    
    file.close();
    std::cout << "配置加载成功: " << configFilePath << std::endl;
    return true;
}

bool ConfigManager::save() {
    std::ofstream file(configFilePath);
    if (!file.is_open()) {
        std::cerr << "无法保存配置文件: " << configFilePath << std::endl;
        return false;
    }
    
    file << "# ConnectTool 配置文件\n\n";
    
    file << "[Window]\n";
    file << "window_width=" << config.windowWidth << "\n";
    file << "window_height=" << config.windowHeight << "\n";
    file << "window_pos_x=" << config.windowPosX << "\n";
    file << "window_pos_y=" << config.windowPosY << "\n\n";
    
    file << "[Network]\n";
    file << "tcp_server_port=" << config.tcpServerPort << "\n\n";
    
    file << "[UI]\n";
    file << "font_size=" << config.fontSize << "\n";
    file << "show_notifications=" << (config.showNotifications ? "1" : "0") << "\n\n";
    
    file << "[History]\n";
    for (uint64_t roomID : config.recentRoomIDs) {
        file << "recent_room=" << roomID << "\n";
    }
    
    file.close();
    return true;
}

void ConfigManager::setWindowSize(int width, int height) {
    config.windowWidth = width;
    config.windowHeight = height;
}

void ConfigManager::setWindowPosition(int x, int y) {
    config.windowPosX = x;
    config.windowPosY = y;
}

void ConfigManager::addRecentRoom(uint64_t roomID) {
    // 移除重复项
    auto it = std::find(config.recentRoomIDs.begin(), config.recentRoomIDs.end(), roomID);
    if (it != config.recentRoomIDs.end()) {
        config.recentRoomIDs.erase(it);
    }
    
    // 添加到开头
    config.recentRoomIDs.insert(config.recentRoomIDs.begin(), roomID);
    
    // 限制数量
    if (config.recentRoomIDs.size() > static_cast<size_t>(config.maxRecentRooms)) {
        config.recentRoomIDs.resize(config.maxRecentRooms);
    }
}

void ConfigManager::clearRecentRooms() {
    config.recentRoomIDs.clear();
}

} // namespace Config
