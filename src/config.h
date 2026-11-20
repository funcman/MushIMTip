#pragma once

#include <Windows.h>

namespace mushimtip {

// 用户可调配置。除自启项（存 HKCU Run 键）外，全部持久化到
// HKCU\Software\MushIMTip。
struct Config {
    bool     followCaret     = true;              // 跟随输入光标显示提示
    bool     sizeFollowCaret = true;              // 提示大小跟随光标高度
    int      scalePct        = 100;               // 缩放百分比（50-200）
    int      alpha           = 180;               // 背景不透明度（20-255）
    int      maxHeightPx     = 24;                // 跟随光标高度时的提示高度上限（96 DPI 像素，12-64）
    COLORREF colorChinese    = RGB(0xE5, 0x39, 0x35); // 红色
    COLORREF colorEnglish    = RGB(0x1E, 0x90, 0xFF); // 蓝色
};

Config LoadConfig();
void   SaveConfig(const Config& config);

// 开机自启动（HKCU\...\CurrentVersion\Run 下的 MushIMTip 值）。
bool IsAutoStartEnabled();
void SetAutoStart(bool enable);

} // namespace mushimtip
