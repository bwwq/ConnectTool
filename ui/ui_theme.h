#pragma once
#include <imgui.h>

namespace UITheme {

// 素色护眼清新主题配色
namespace Colors {
    // 背景色 - 柔和的米白色系
    const ImVec4 Background = ImVec4(0.96f, 0.96f, 0.94f, 1.00f);          // 主背景
    const ImVec4 BackgroundDark = ImVec4(0.92f, 0.92f, 0.90f, 1.00f);      // 深一点的背景
    const ImVec4 BackgroundLight = ImVec4(0.98f, 0.98f, 0.97f, 1.00f);     // 浅背景
    
    // 文字色 - 深灰色，减少对比度
    const ImVec4 Text = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);                // 主文字
    const ImVec4 TextDisabled = ImVec4(0.60f, 0.60f, 0.58f, 1.00f);        // 禁用文字
    const ImVec4 TextSecondary = ImVec4(0.45f, 0.45f, 0.43f, 1.00f);       // 次要文字
    
    // 主题色 - 柔和的绿色（护眼）
    const ImVec4 Primary = ImVec4(0.52f, 0.73f, 0.62f, 1.00f);             // 主题绿
    const ImVec4 PrimaryHovered = ImVec4(0.60f, 0.80f, 0.70f, 1.00f);      // 悬停
    const ImVec4 PrimaryActive = ImVec4(0.45f, 0.65f, 0.55f, 1.00f);       // 激活
    
    // 辅助色
    const ImVec4 Success = ImVec4(0.56f, 0.76f, 0.56f, 1.00f);             // 成功绿
    const ImVec4 Warning = ImVec4(0.90f, 0.78f, 0.50f, 1.00f);             // 警告黄
    const ImVec4 Error = ImVec4(0.88f, 0.58f, 0.56f, 1.00f);               // 错误红
    const ImVec4 Info = ImVec4(0.60f, 0.75f, 0.88f, 1.00f);                // 信息蓝
    
    // UI元素
    const ImVec4 Border = ImVec4(0.80f, 0.80f, 0.78f, 1.00f);              // 边框
    const ImVec4 BorderLight = ImVec4(0.88f, 0.88f, 0.86f, 1.00f);         // 浅边框
    const ImVec4 Separator = ImVec4(0.75f, 0.75f, 0.73f, 0.60f);           // 分隔线
    
    // 窗口和面板
    const ImVec4 WindowBg = ImVec4(0.95f, 0.95f, 0.93f, 0.98f);            // 窗口背景
    const ImVec4 ChildBg = ImVec4(0.97f, 0.97f, 0.95f, 1.00f);             // 子窗口背景
    const ImVec4 PopupBg = ImVec4(0.98f, 0.98f, 0.97f, 0.98f);             // 弹出窗口
    
    // 标题栏
    const ImVec4 TitleBg = ImVec4(0.88f, 0.88f, 0.86f, 1.00f);             // 非激活标题
    const ImVec4 TitleBgActive = ImVec4(0.70f, 0.82f, 0.75f, 1.00f);       // 激活标题
    const ImVec4 TitleBgCollapsed = ImVec4(0.90f, 0.90f, 0.88f, 0.75f);    // 折叠标题
    
    // 滚动条
    const ImVec4 ScrollbarBg = ImVec4(0.93f, 0.93f, 0.91f, 0.60f);         // 滚动条背景
    const ImVec4 ScrollbarGrab = ImVec4(0.75f, 0.75f, 0.73f, 1.00f);       // 滑块
    const ImVec4 ScrollbarGrabHovered = ImVec4(0.65f, 0.65f, 0.63f, 1.00f);
    const ImVec4 ScrollbarGrabActive = ImVec4(0.55f, 0.55f, 0.53f, 1.00f);
    
    // 按钮
    const ImVec4 Button = ImVec4(0.85f, 0.85f, 0.83f, 1.00f);              // 按钮
    const ImVec4 ButtonHovered = ImVec4(0.65f, 0.78f, 0.70f, 1.00f);       // 悬停
    const ImVec4 ButtonActive = ImVec4(0.52f, 0.73f, 0.62f, 1.00f);        // 按下
    
    // 头部（表格等）
    const ImVec4 Header = ImVec4(0.78f, 0.85f, 0.80f, 1.00f);              // 表头
    const ImVec4 HeaderHovered = ImVec4(0.70f, 0.80f, 0.73f, 1.00f);
    const ImVec4 HeaderActive = ImVec4(0.65f, 0.75f, 0.68f, 1.00f);
    
    // 输入框
    const ImVec4 FrameBg = ImVec4(0.98f, 0.98f, 0.97f, 1.00f);             // 输入框背景
    const ImVec4 FrameBgHovered = ImVec4(0.95f, 0.95f, 0.93f, 1.00f);
    const ImVec4 FrameBgActive = ImVec4(0.93f, 0.93f, 0.91f, 1.00f);
    
    // 选择和勾选
    const ImVec4 CheckMark = ImVec4(0.52f, 0.73f, 0.62f, 1.00f);           // 勾选标记
    const ImVec4 SliderGrab = ImVec4(0.60f, 0.76f, 0.67f, 1.00f);          // 滑块抓手
    const ImVec4 SliderGrabActive = ImVec4(0.52f, 0.73f, 0.62f, 1.00f);
    
    // Tab 标签
    const ImVec4 Tab = ImVec4(0.85f, 0.85f, 0.83f, 1.00f);
    const ImVec4 TabHovered = ImVec4(0.70f, 0.80f, 0.73f, 1.00f);
    const ImVec4 TabActive = ImVec4(0.78f, 0.85f, 0.80f, 1.00f);
}

// 应用素色护眼主题
inline void ApplyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // 设置颜色
    colors[ImGuiCol_Text]                   = Colors::Text;
    colors[ImGuiCol_TextDisabled]           = Colors::TextDisabled;
    colors[ImGuiCol_WindowBg]               = Colors::WindowBg;
    colors[ImGuiCol_ChildBg]                = Colors::ChildBg;
    colors[ImGuiCol_PopupBg]                = Colors::PopupBg;
    colors[ImGuiCol_Border]                 = Colors::Border;
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = Colors::FrameBg;
    colors[ImGuiCol_FrameBgHovered]         = Colors::FrameBgHovered;
    colors[ImGuiCol_FrameBgActive]          = Colors::FrameBgActive;
    colors[ImGuiCol_TitleBg]                = Colors::TitleBg;
    colors[ImGuiCol_TitleBgActive]          = Colors::TitleBgActive;
    colors[ImGuiCol_TitleBgCollapsed]       = Colors::TitleBgCollapsed;
    colors[ImGuiCol_MenuBarBg]              = Colors::BackgroundDark;
    colors[ImGuiCol_ScrollbarBg]            = Colors::ScrollbarBg;
    colors[ImGuiCol_ScrollbarGrab]          = Colors::ScrollbarGrab;
    colors[ImGuiCol_ScrollbarGrabHovered]   = Colors::ScrollbarGrabHovered;
    colors[ImGuiCol_ScrollbarGrabActive]    = Colors::ScrollbarGrabActive;
    colors[ImGuiCol_CheckMark]              = Colors::CheckMark;
    colors[ImGuiCol_SliderGrab]             = Colors::SliderGrab;
    colors[ImGuiCol_SliderGrabActive]       = Colors::SliderGrabActive;
    colors[ImGuiCol_Button]                 = Colors::Button;
    colors[ImGuiCol_ButtonHovered]          = Colors::ButtonHovered;
    colors[ImGuiCol_ButtonActive]           = Colors::ButtonActive;
    colors[ImGuiCol_Header]                 = Colors::Header;
    colors[ImGuiCol_HeaderHovered]          = Colors::HeaderHovered;
    colors[ImGuiCol_HeaderActive]           = Colors::HeaderActive;
    colors[ImGuiCol_Separator]              = Colors::Separator;
    colors[ImGuiCol_SeparatorHovered]       = Colors::Primary;
    colors[ImGuiCol_SeparatorActive]        = Colors::PrimaryActive;
    colors[ImGuiCol_ResizeGrip]             = Colors::Primary;
    colors[ImGuiCol_ResizeGripHovered]      = Colors::PrimaryHovered;
    colors[ImGuiCol_ResizeGripActive]       = Colors::PrimaryActive;
    colors[ImGuiCol_Tab]                    = Colors::Tab;
    colors[ImGuiCol_TabHovered]             = Colors::TabHovered;
    colors[ImGuiCol_TabActive]              = Colors::TabActive;
    colors[ImGuiCol_TabUnfocused]           = Colors::Tab;
    colors[ImGuiCol_TabUnfocusedActive]     = Colors::TabActive;
    colors[ImGuiCol_PlotLines]              = Colors::Primary;
    colors[ImGuiCol_PlotLinesHovered]       = Colors::PrimaryHovered;
    colors[ImGuiCol_PlotHistogram]          = Colors::Primary;
    colors[ImGuiCol_PlotHistogramHovered]   = Colors::PrimaryHovered;
    colors[ImGuiCol_TableHeaderBg]          = Colors::Header;
    colors[ImGuiCol_TableBorderStrong]      = Colors::Border;
    colors[ImGuiCol_TableBorderLight]       = Colors::BorderLight;
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(0.30f, 0.30f, 0.30f, 0.05f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.52f, 0.73f, 0.62f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = Colors::Primary;
    colors[ImGuiCol_NavHighlight]           = Colors::Primary;
    colors[ImGuiCol_NavWindowingHighlight]  = Colors::Primary;
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    
    // 设置样式参数 - 圆角和间距
    style.WindowRounding    = 6.0f;     // 窗口圆角
    style.ChildRounding     = 4.0f;     // 子窗口圆角
    style.FrameRounding     = 4.0f;     // 输入框圆角
    style.PopupRounding     = 4.0f;     // 弹出窗口圆角
    style.ScrollbarRounding = 6.0f;     // 滚动条圆角
    style.GrabRounding      = 4.0f;     // 滑块圆角
    style.TabRounding       = 4.0f;     // Tab 圆角
    
    style.WindowPadding     = ImVec2(12.0f, 12.0f);    // 窗口内边距
    style.FramePadding      = ImVec2(8.0f, 4.0f);      // 控件内边距
    style.ItemSpacing       = ImVec2(10.0f, 6.0f);     // 元素间距
    style.ItemInnerSpacing  = ImVec2(6.0f, 4.0f);      // 元素内部间距
    style.IndentSpacing     = 20.0f;                    // 缩进距离
    
    style.ScrollbarSize     = 14.0f;    // 滚动条大小
    style.GrabMinSize       = 10.0f;    // 滑块最小大小
    
    style.WindowBorderSize  = 1.0f;     // 窗口边框
    style.ChildBorderSize   = 1.0f;     // 子窗口边框
    style.PopupBorderSize   = 1.0f;     // 弹出窗口边框
    style.FrameBorderSize   = 1.0f;     // 输入框边框
    style.TabBorderSize     = 0.0f;     // Tab 边框
    
    // 对齐方式
    style.WindowTitleAlign  = ImVec2(0.5f, 0.5f);      // 标题居中
    style.ButtonTextAlign   = ImVec2(0.5f, 0.5f);      // 按钮文字居中
}

} // namespace UITheme
