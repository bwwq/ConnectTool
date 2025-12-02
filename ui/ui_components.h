#pragma once
#include <imgui.h>
#include <string>
#include <functional>
#include "ui_theme.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace UIComponents {

// 通知类型
enum class NotificationType {
    Success,
    Warning,
    Error,
    Info
};

// 通知项
struct Notification {
    std::string message;
    NotificationType type;
    float displayTime;
    float currentTime;
    
    Notification(const std::string& msg, NotificationType t, float duration = 3.0f)
        : message(msg), type(t), displayTime(duration), currentTime(0.0f) {}
};

// 通知管理器
class NotificationManager {
private:
    std::vector<Notification> notifications;
    
public:
    void addNotification(const std::string& message, NotificationType type, float duration = 3.0f) {
        notifications.emplace_back(message, type, duration);
    }
    
    void update(float deltaTime) {
        for (auto it = notifications.begin(); it != notifications.end(); ) {
            it->currentTime += deltaTime;
            if (it->currentTime >= it->displayTime) {
                it = notifications.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    void render() {
        if (notifications.empty()) return;
        
        const float DISTANCE = 10.0f;
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 window_pos = ImVec2(io.DisplaySize.x - DISTANCE, DISTANCE);
        
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.95f);
        
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
        
        if (ImGui::Begin("Notifications", nullptr, flags)) {
            for (const auto& notif : notifications) {
                ImVec4 color;
                const char* icon;
                
                switch (notif.type) {
                    case NotificationType::Success:
                        color = UITheme::Colors::Success;
                        icon = "[✓] ";
                        break;
                    case NotificationType::Warning:
                        color = UITheme::Colors::Warning;
                        icon = "[!] ";
                        break;
                    case NotificationType::Error:
                        color = UITheme::Colors::Error;
                        icon = "[✗] ";
                        break;
                    case NotificationType::Info:
                    default:
                        color = UITheme::Colors::Info;
                        icon = "[i] ";
                        break;
                }
                
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextWrapped("%s%s", icon, notif.message.c_str());
                ImGui::PopStyleColor();
                
                // 进度条
                float progress = notif.currentTime / notif.displayTime;
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
                ImGui::ProgressBar(progress, ImVec2(-1.0f, 2.0f), "");
                ImGui::PopStyleColor();
                
                if (&notif != &notifications.back()) {
                    ImGui::Spacing();
                }
            }
        }
        ImGui::End();
    }
};

// 状态指示器
inline void StatusIndicator(const char* label, bool connected, int ping = -1) {
    ImGui::BeginGroup();
    
    // 状态点
    ImVec4 color = connected ? UITheme::Colors::Success : UITheme::Colors::TextDisabled;
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::Text("●");
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    ImGui::Text("%s", label);
    
    if (connected && ping >= 0) {
        ImGui::SameLine();
        ImGui::TextColored(UITheme::Colors::TextSecondary, "(%d ms)", ping);
    }
    
    ImGui::EndGroup();
}

// 可复制的文本框（用于房间ID等）
inline bool CopyableText(const char* label, const char* text, const char* buttonText = "复制") {
    bool copied = false;
    
    ImGui::BeginGroup();
    ImGui::Text("%s", label);
    ImGui::SameLine();
    
    // 显示文本（带边框）
    ImGui::PushStyleColor(ImGuiCol_FrameBg, UITheme::Colors::BackgroundLight);
    ImGui::PushStyleColor(ImGuiCol_Border, UITheme::Colors::Primary);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));
    
    ImGui::InputText(("##" + std::string(label)).c_str(), 
                     const_cast<char*>(text), 
                     strlen(text), 
                     ImGuiInputTextFlags_ReadOnly);
    
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    
    ImGui::SameLine();
    
    // 复制按钮
    ImGui::PushStyleColor(ImGuiCol_Button, UITheme::Colors::Primary);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UITheme::Colors::PrimaryHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, UITheme::Colors::PrimaryActive);
    
    if (ImGui::Button(buttonText)) {
        #ifdef _WIN32
        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            HGLOBAL hGlob = GlobalAlloc(GMEM_FIXED, strlen(text) + 1);
            if (hGlob) {
                strcpy_s(static_cast<char*>(hGlob), strlen(text) + 1, text);
                SetClipboardData(CF_TEXT, hGlob);
            }
            CloseClipboard();
            copied = true;
        }
        #endif
    }
    
    ImGui::PopStyleColor(3);
    ImGui::EndGroup();
    
    return copied;
}

// 卡片式容器
inline bool BeginCard(const char* label, const ImVec2& size = ImVec2(0, 0)) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, UITheme::Colors::ChildBg);
    ImGui::PushStyleColor(ImGuiCol_Border, UITheme::Colors::BorderLight);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    
    ImGui::BeginChild(label, size, true);
    return true;
}

inline void EndCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// 大按钮（主要操作）
inline bool BigButton(const char* label, const ImVec2& size = ImVec2(0, 0)) {
    ImGui::PushStyleColor(ImGuiCol_Button, UITheme::Colors::Primary);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UITheme::Colors::PrimaryHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, UITheme::Colors::PrimaryActive);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 8.0f));
    
    ImVec2 buttonSize = size;
    if (buttonSize.x == 0) {
        buttonSize.x = ImGui::GetContentRegionAvail().x;
    }
    if (buttonSize.y == 0) {
        buttonSize.y = 40.0f;
    }
    
    bool result = ImGui::Button(label, buttonSize);
    
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
    
    return result;
}

// 次要按钮
inline bool SecondaryButton(const char* label, const ImVec2& size = ImVec2(0, 0)) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    
    bool result = ImGui::Button(label, size);
    
    ImGui::PopStyleVar();
    
    return result;
}

// 危险按钮（断开连接等）
inline bool DangerButton(const char* label, const ImVec2& size = ImVec2(0, 0)) {
    ImGui::PushStyleColor(ImGuiCol_Button, UITheme::Colors::Error);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.92f, 0.62f, 0.60f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.82f, 0.52f, 0.50f, 1.00f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    
    bool result = ImGui::Button(label, size);
    
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    
    return result;
}

// 标题文本
inline void TitleText(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, UITheme::Colors::Text);
    ImGui::PushFont(ImGui::GetFont()); // 可以在这里加载更大的字体
    ImGui::Text("%s", text);
    ImGui::PopFont();
    ImGui::PopStyleColor();
}

// 辅助文本
inline void HelpText(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, UITheme::Colors::TextSecondary);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

// 分隔线（带文字）
inline void SeparatorWithText(const char* text) {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    ImVec2 textSize = ImGui::CalcTextSize(text);
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    
    ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + (width - textSize.x) * 0.5f, cursorPos.y));
    ImGui::PushStyleColor(ImGuiCol_Text, UITheme::Colors::TextSecondary);
    ImGui::Text("%s", text);
    ImGui::PopStyleColor();
    
    ImGui::Spacing();
}

} // namespace UIComponents
