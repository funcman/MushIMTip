# MushIMTip

跟随输入光标显示当前输入法状态（中 / 英）的 Win32 小工具，纯 Win32 + IMM 实现，无任何第三方依赖。

## 功能

- 在文本光标旁实时显示当前输入法状态：`中`（IME 中文模式）/ `英`（英文模式或无 IME 布局）
- 光标消失时提示自动隐藏
- 系统托盘图标 + 右键菜单：
  - **跟随输入光标**：启停提示
  - **开机自启动**：写入 / 移除 `HKCU\...\Run` 注册表项
  - **退出**
- Per-Monitor V2 DPI 感知；单实例运行

## 实现原理

每 60ms 轮询一次，光标位置按以下顺序逐级回退检测（前一个拿不到就用下一个）：

1. **系统光标**（`GetGUIThreadInfo` → `hwndCaret` / `rcCaret`）：传统 Win32 控件。`GUI_CARETBLINKING` 标志随闪烁相位开合，因此加了一个闪烁周期的宽限期，避免提示跟着光标一起闪
2. **UI Automation**（`TextPattern2::GetCaretRange` + 包围矩形）：Chrome/Edge/Electron、新版记事本、WPF 等自绘光标的应用
3. **MSAA**（`AccessibleObjectFromWindow` + `OBJID_CARET` + `accLocation`）：只走 MSAA 辅助功能通道的老牌应用

IME 状态通过 `ImmGetDefaultIMEWnd` + `WM_IME_CONTROL`（`IMC_GETOPENSTATUS` / `IMC_GETCONVERSIONMODE`）查询；无 IME 的键盘布局（如纯英文）显示「英」。

提示窗为置顶、不可激活、点击穿透的分层（`WS_EX_LAYERED`）小窗，逐像素透明渲染：圆角背景半透明（约 70% 不透明度），文字与边框保持清晰，贴近屏幕边缘时自动翻转到光标另一侧。

已知限制：

- 对以更高权限（管理员）运行的目标进程，跨进程查询会被拒绝，提示自动隐藏
- TSF（`ITfContextView::GetTextExt`）只能查询调用线程自身的上下文，跨进程不可用，故未采用
- 个别完全自绘且不暴露任何辅助功能接口的控件（如某些 IM 输入框）无法定位光标

## 构建

需要 Windows + CMake + Visual Studio（MSVC）。cmake 优先取 PATH，其次自动探测 Visual Studio 自带的 CMake。

```powershell
script\build.ps1                # Release 构建
script\build.ps1 -Config Debug  # Debug 构建
script\build.ps1 -Clean         # 清理后重新构建
script\build.ps1 -CleanOnly     # 只清理不构建
```

产物：`bin\<Config>\MushIMTip.exe`

## 目录结构

```
src/      源代码（main / tip_window / ime）
script/   构建脚本
build/    CMake 构建目录（git 忽略）
bin/      构建产物（git 忽略）
```
