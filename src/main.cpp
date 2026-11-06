#include <Windows.h>
#include <objbase.h> // CoInitializeEx / CoUninitialize（WIN32_LEAN_AND_MEAN 不带）
#include <shellapi.h>

#include <iterator>

#include "ime.h"
#include "tip_window.h"

namespace mushimtip {

namespace {

constexpr wchar_t kAppName[]   = L"MushIMTip";
constexpr wchar_t kClassName[] = L"MushIMTipMessageWindow";
constexpr wchar_t kRunKey[]    = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

constexpr UINT     kTrayMessage    = WM_APP + 1;
constexpr UINT_PTR kTimerId        = 1;
constexpr UINT     kPollIntervalMs = 60;

constexpr WORD IDM_TOGGLE    = 1001; // enable / pause the tip
constexpr WORD IDM_AUTOSTART = 1002; // launch on user logon
constexpr WORD IDM_EXIT      = 1003;

HINSTANCE g_hInstance = nullptr;
HWND      g_hwndMsg   = nullptr;
HICON     g_trayIcon  = nullptr;
UINT      g_taskbarCreated = 0;
TipWindow g_tip;
bool      g_enabled = true;

// ---------------------------------------------------------------- DPI

void EnableDpiAwareness() {
    // Prefer Per-Monitor V2 (Win10 1703+), fall back to system awareness.
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        using SetContextFn = BOOL(WINAPI*)(HANDLE);
        if (auto fn = reinterpret_cast<SetContextFn>(
                GetProcAddress(user32, "SetProcessDpiAwarenessContext"))) {
            if (fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
                return;
        }
    }
    SetProcessDPIAware();
}

// ------------------------------------------------------------ autostart

bool IsAutoStartEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return false;
    DWORD type = 0;
    DWORD size = 0;
    bool present = RegQueryValueExW(key, kAppName, nullptr, &type, nullptr, &size) == ERROR_SUCCESS;
    RegCloseKey(key);
    return present;
}

void SetAutoStart(bool enable) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return;
    if (enable) {
        wchar_t path[MAX_PATH + 2] = {};
        GetModuleFileNameW(nullptr, path + 1, MAX_PATH);
        path[0] = L'"';
        size_t len = wcslen(path);
        path[len] = L'"';
        path[len + 1] = L'\0';
        RegSetValueExW(key, kAppName, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(path),
                       static_cast<DWORD>((wcslen(path) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, kAppName);
    }
    RegCloseKey(key);
}

// ---------------------------------------------------------- tray icon

// Draws the tray icon at runtime (red square with a white "中"),
// so the binary needs no embedded resources.
HICON CreateTrayIcon() {
    const int size = GetSystemMetrics(SM_CXSMICON);

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcColor  = CreateCompatibleDC(hdcScreen);
    HDC hdcMask   = CreateCompatibleDC(hdcScreen);

    HBITMAP hbmColor = CreateCompatibleBitmap(hdcScreen, size, size);
    HBITMAP hbmMask  = CreateBitmap(size, size, 1, 1, nullptr);

    HGDIOBJ oldColor = SelectObject(hdcColor, hbmColor);
    RECT rc{ 0, 0, size, size };
    HBRUSH bg = CreateSolidBrush(RGB(0xE5, 0x39, 0x35));
    FillRect(hdcColor, &rc, bg);
    DeleteObject(bg);

    HFONT font = CreateFontW(-MulDiv(11, size, 16), 0, 0, 0, FW_BOLD,
                             FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                             L"Microsoft YaHei UI");
    HGDIOBJ oldFont = SelectObject(hdcColor, font);
    SetBkMode(hdcColor, TRANSPARENT);
    SetTextColor(hdcColor, RGB(0xFF, 0xFF, 0xFF));
    DrawTextW(hdcColor, L"中", 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdcColor, oldFont);
    DeleteObject(font);
    SelectObject(hdcColor, oldColor);

    // AND mask: 0 bits keep the color bitmap -> fully opaque icon.
    HGDIOBJ oldMask = SelectObject(hdcMask, hbmMask);
    BitBlt(hdcMask, 0, 0, size, size, nullptr, 0, 0, BLACKNESS);
    SelectObject(hdcMask, oldMask);

    DeleteDC(hdcMask);
    DeleteDC(hdcColor);
    ReleaseDC(nullptr, hdcScreen);

    ICONINFO ii{};
    ii.fIcon    = TRUE;
    ii.hbmMask  = hbmMask;
    ii.hbmColor = hbmColor;
    HICON icon = CreateIconIndirect(&ii);

    DeleteObject(hbmColor);
    DeleteObject(hbmMask);
    return icon;
}

void TrayAdd() {
    NOTIFYICONDATAW nid{ sizeof(nid) };
    nid.hWnd             = g_hwndMsg;
    nid.uID              = 1;
    nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = kTrayMessage;
    nid.hIcon            = g_trayIcon;
    lstrcpynW(nid.szTip, L"MushIMTip - 输入法状态提示", static_cast<int>(std::size(nid.szTip)));
    Shell_NotifyIconW(NIM_ADD, &nid);
    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid);
}

void TrayRemove() {
    NOTIFYICONDATAW nid{ sizeof(nid) };
    nid.hWnd = g_hwndMsg;
    nid.uID  = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

// ------------------------------------------------------------- polling

void Poll() {
    if (!g_enabled) {
        g_tip.Hide();
        return;
    }

    CaretPos caret = QueryCaretPos();
    if (!caret.valid) {
        g_tip.Hide();
        return;
    }

    g_tip.ShowState(QueryImeState(), caret);
}

// --------------------------------------------------------------- menu

void ShowTrayMenu() {
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;

    AppendMenuW(menu, MF_STRING | (g_enabled ? MF_CHECKED : 0),
                IDM_TOGGLE, L"跟随输入光标");
    AppendMenuW(menu, MF_STRING | (IsAutoStartEnabled() ? MF_CHECKED : 0),
                IDM_AUTOSTART, L"开机自启动");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"退出");

    POINT pt;
    GetCursorPos(&pt);

    // Required so the menu dismisses when clicking elsewhere.
    SetForegroundWindow(g_hwndMsg);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwndMsg, nullptr);
    PostMessageW(g_hwndMsg, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

// ---------------------------------------------------------- window proc

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == g_taskbarCreated) {
        // Explorer restarted: re-register the tray icon.
        TrayAdd();
        return 0;
    }

    switch (msg) {
    case WM_TIMER:
        if (wParam == kTimerId)
            Poll();
        return 0;

    case kTrayMessage:
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU)
            ShowTrayMenu();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_TOGGLE:
            g_enabled = !g_enabled;
            if (!g_enabled)
                g_tip.Hide();
            return 0;
        case IDM_AUTOSTART:
            SetAutoStart(!IsAutoStartEnabled());
            return 0;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, kTimerId);
        TrayRemove();
        g_tip.Destroy();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

// ------------------------------------------------------------- entry

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    EnableDpiAwareness();

    // UI Automation fallback needs COM on this thread.
    HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\MushIMTip.SingleInstance");
    if (!mutex)
        return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return 0; // already running
    }

    g_hInstance = hInstance;
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    if (!g_tip.Create(hInstance))
        return 1;

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &MainWndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc))
        return 1;

    g_hwndMsg = CreateWindowExW(0, kClassName, kAppName, 0,
                                0, 0, 0, 0,
                                HWND_MESSAGE, nullptr, hInstance, nullptr);
    if (!g_hwndMsg)
        return 1;

    g_trayIcon = CreateTrayIcon();
    TrayAdd();
    SetTimer(g_hwndMsg, kTimerId, kPollIntervalMs, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_trayIcon)
        DestroyIcon(g_trayIcon);
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    if (SUCCEEDED(comHr))
        CoUninitialize();
    return 0;
}

} // namespace mushimtip

// Global entry point expected by the linker (wWinMainCRTStartup).
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR lpCmdLine, int nCmdShow) {
    return mushimtip::wWinMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}
