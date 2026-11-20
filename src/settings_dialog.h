#pragma once

#include <Windows.h>

#include "config.h"

namespace mushimtip {

// 配置变更回调：对话框中任一设置变化即触发（所见即所得）。
using SettingsApplyCallback = void (*)(const Config& config, bool autoStart, void* userData);

// 打开（或激活已打开的）配置窗口。非模态，全局最多一个实例。
void ShowSettingsDialog(HINSTANCE hInstance, HWND owner,
                        const Config& config, bool autoStart,
                        SettingsApplyCallback onApply, void* userData);

} // namespace mushimtip
