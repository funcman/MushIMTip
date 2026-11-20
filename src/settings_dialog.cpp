#include "settings_dialog.h"

#include <commctrl.h>
#include <commdlg.h>

namespace mushimtip {

namespace {

constexpr wchar_t kClassName[] = L"MushIMTipSettingsDialog";

// 控件 ID
enum ControlId {
    ID_CHK_FOLLOW     = 201,
    ID_CHK_AUTOSTART  = 202,
    ID_CHK_SIZEFOLLOW = 203,
    ID_TRK_SCALE      = 204,
    ID_LBL_SCALE      = 205,
    ID_TRK_ALPHA      = 206,
    ID_LBL_ALPHA      = 207,
    ID_BTN_COLOR_CN   = 208,
    ID_BTN_COLOR_EN   = 209,
    ID_BTN_RESET      = 210,
    ID_BTN_CLOSE      = 211,
    ID_TRK_MAXH       = 212,
    ID_LBL_MAXH       = 213,
};

constexpr int kScaleMin = 50;
constexpr int kScaleMax = 200;
constexpr int kAlphaMin = 20;
constexpr int kAlphaMax = 255;
constexpr int kMaxHMin  = 12;
constexpr int kMaxHMax  = 64;

struct DialogState {
    Config config;
    bool   autoStart = false;
    SettingsApplyCallback onApply = nullptr;
    void*  userData = nullptr;
};

HWND        s_hwnd = nullptr;
DialogState s_state;
COLORREF    s_customColors[16] = {}; // ChooseColor 的自定义色（进程内保持）

void Apply() {
    if (s_state.onApply)
        s_state.onApply(s_state.config, s_state.autoStart, s_state.userData);
}

void UpdateValueLabels(HWND hwnd) {
    wchar_t buf[32];

    HWND lbl = GetDlgItem(hwnd, ID_LBL_SCALE);
    wsprintfW(buf, L"%d%%", s_state.config.scalePct);
    SetWindowTextW(lbl, buf);

    lbl = GetDlgItem(hwnd, ID_LBL_ALPHA);
    wsprintfW(buf, L"%d（%d%%）", s_state.config.alpha, s_state.config.alpha * 100 / 255);
    SetWindowTextW(lbl, buf);

    lbl = GetDlgItem(hwnd, ID_LBL_MAXH);
    wsprintfW(buf, L"%d px", s_state.config.maxHeightPx);
    SetWindowTextW(lbl, buf);
}

// 把控件状态同步为 s_state 的当前值（初始化 / 恢复默认时用）。
void RefreshControls(HWND hwnd) {
    SendDlgItemMessageW(hwnd, ID_CHK_FOLLOW, BM_SETCHECK,
                        s_state.config.followCaret ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageW(hwnd, ID_CHK_AUTOSTART, BM_SETCHECK,
                        s_state.autoStart ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageW(hwnd, ID_CHK_SIZEFOLLOW, BM_SETCHECK,
                        s_state.config.sizeFollowCaret ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessageW(hwnd, ID_TRK_SCALE, TBM_SETPOS, TRUE, s_state.config.scalePct);
    SendDlgItemMessageW(hwnd, ID_TRK_ALPHA, TBM_SETPOS, TRUE, s_state.config.alpha);
    SendDlgItemMessageW(hwnd, ID_TRK_MAXH, TBM_SETPOS, TRUE, s_state.config.maxHeightPx);
    UpdateValueLabels(hwnd);
    InvalidateRect(GetDlgItem(hwnd, ID_BTN_COLOR_CN), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, ID_BTN_COLOR_EN), nullptr, TRUE);
}

bool PickColor(HWND hwnd, COLORREF& color) {
    CHOOSECOLORW cc{ sizeof(cc) };
    cc.hwndOwner      = hwnd;
    cc.lpCustColors   = s_customColors;
    cc.rgbResult      = color;
    cc.Flags          = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColorW(&cc))
        return false;
    color = cc.rgbResult;
    return true;
}

HWND MakeChild(HWND hwnd, HINSTANCE inst, int id, const wchar_t* cls,
               const wchar_t* text, DWORD style, DWORD exStyle,
               int x, int y, int w, int h) {
    return CreateWindowExW(exStyle, cls, text, style | WS_CHILD | WS_VISIBLE,
                           x, y, w, h, hwnd,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                           inst, nullptr);
}

void OnCreate(HWND hwnd) {
    HINSTANCE inst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
    HFONT guiFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    const DWORD chk = BS_AUTOCHECKBOX | WS_TABSTOP;
    const DWORD trk = TBS_AUTOTICKS | WS_TABSTOP;
    int y = 12;

    MakeChild(hwnd, inst, ID_CHK_FOLLOW, L"BUTTON", L"跟随输入光标显示提示", chk, 0, 16, y, 250, 22); y += 28;
    MakeChild(hwnd, inst, ID_CHK_AUTOSTART, L"BUTTON", L"开机自启动", chk, 0, 16, y, 250, 22); y += 28;
    MakeChild(hwnd, inst, ID_CHK_SIZEFOLLOW, L"BUTTON", L"提示大小跟随光标高度", chk, 0, 16, y, 250, 22); y += 32;

    MakeChild(hwnd, inst, 0, L"STATIC", L"缩放", 0, 0, 16, y + 4, 60, 18);
    MakeChild(hwnd, inst, ID_TRK_SCALE, TRACKBAR_CLASSW, nullptr, trk, 0, 76, y, 150, 26);
    MakeChild(hwnd, inst, ID_LBL_SCALE, L"STATIC", L"", 0, 0, 232, y + 4, 76, 18);
    y += 36;

    MakeChild(hwnd, inst, 0, L"STATIC", L"不透明度", 0, 0, 16, y + 4, 60, 18);
    MakeChild(hwnd, inst, ID_TRK_ALPHA, TRACKBAR_CLASSW, nullptr, trk, 0, 76, y, 150, 26);
    MakeChild(hwnd, inst, ID_LBL_ALPHA, L"STATIC", L"", 0, 0, 232, y + 4, 86, 18);
    y += 36;

    MakeChild(hwnd, inst, 0, L"STATIC", L"跟随高度上限", 0, 0, 16, y + 4, 76, 18);
    MakeChild(hwnd, inst, ID_TRK_MAXH, TRACKBAR_CLASSW, nullptr, trk, 0, 96, y, 130, 26);
    MakeChild(hwnd, inst, ID_LBL_MAXH, L"STATIC", L"", 0, 0, 232, y + 4, 86, 18);
    y += 36;

    MakeChild(hwnd, inst, 0, L"STATIC", L"中文状态颜色", 0, 0, 16, y + 3, 110, 18);
    MakeChild(hwnd, inst, ID_BTN_COLOR_CN, L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, 0, 132, y, 60, 24);
    y += 34;

    MakeChild(hwnd, inst, 0, L"STATIC", L"英文状态颜色", 0, 0, 16, y + 3, 110, 18);
    MakeChild(hwnd, inst, ID_BTN_COLOR_EN, L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, 0, 132, y, 60, 24);
    y += 40;

    MakeChild(hwnd, inst, ID_BTN_RESET, L"BUTTON", L"恢复默认", BS_PUSHBUTTON | WS_TABSTOP, 0, 16, y, 80, 26);
    MakeChild(hwnd, inst, ID_BTN_CLOSE, L"BUTTON", L"关闭", BS_DEFPUSHBUTTON | WS_TABSTOP, 0, 228, y, 80, 26);

    // 统一字体
    EnumChildWindows(hwnd, [](HWND child, LPARAM font) -> BOOL {
        SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
        return TRUE;
    }, reinterpret_cast<LPARAM>(guiFont));

    // 滑块量程
    SendDlgItemMessageW(hwnd, ID_TRK_SCALE, TBM_SETRANGE, TRUE, MAKELPARAM(kScaleMin, kScaleMax));
    SendDlgItemMessageW(hwnd, ID_TRK_ALPHA, TBM_SETRANGE, TRUE, MAKELPARAM(kAlphaMin, kAlphaMax));
    SendDlgItemMessageW(hwnd, ID_TRK_MAXH, TBM_SETRANGE, TRUE, MAKELPARAM(kMaxHMin, kMaxHMax));

    RefreshControls(hwnd);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        OnCreate(hwnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_CHK_FOLLOW:
            s_state.config.followCaret =
                SendDlgItemMessageW(hwnd, ID_CHK_FOLLOW, BM_GETCHECK, 0, 0) == BST_CHECKED;
            Apply();
            return 0;
        case ID_CHK_AUTOSTART:
            s_state.autoStart =
                SendDlgItemMessageW(hwnd, ID_CHK_AUTOSTART, BM_GETCHECK, 0, 0) == BST_CHECKED;
            Apply();
            return 0;
        case ID_CHK_SIZEFOLLOW:
            s_state.config.sizeFollowCaret =
                SendDlgItemMessageW(hwnd, ID_CHK_SIZEFOLLOW, BM_GETCHECK, 0, 0) == BST_CHECKED;
            Apply();
            return 0;
        case ID_BTN_COLOR_CN:
            if (PickColor(hwnd, s_state.config.colorChinese)) {
                InvalidateRect(GetDlgItem(hwnd, ID_BTN_COLOR_CN), nullptr, TRUE);
                Apply();
            }
            return 0;
        case ID_BTN_COLOR_EN:
            if (PickColor(hwnd, s_state.config.colorEnglish)) {
                InvalidateRect(GetDlgItem(hwnd, ID_BTN_COLOR_EN), nullptr, TRUE);
                Apply();
            }
            return 0;
        case ID_BTN_RESET: {
            bool follow = s_state.config.followCaret; // 开关类设置不重置
            s_state.config = Config{};
            s_state.config.followCaret = follow;
            RefreshControls(hwnd);
            Apply();
            return 0;
        }
        case ID_BTN_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_HSCROLL: {
        HWND track = reinterpret_cast<HWND>(lParam);
        int pos = static_cast<int>(SendMessageW(track, TBM_GETPOS, 0, 0));
        if (GetDlgCtrlID(track) == ID_TRK_SCALE) {
            s_state.config.scalePct = pos;
            UpdateValueLabels(hwnd);
            Apply();
        } else if (GetDlgCtrlID(track) == ID_TRK_ALPHA) {
            s_state.config.alpha = pos;
            UpdateValueLabels(hwnd);
            Apply();
        } else if (GetDlgCtrlID(track) == ID_TRK_MAXH) {
            s_state.config.maxHeightPx = pos;
            UpdateValueLabels(hwnd);
            Apply();
        }
        return 0;
    }

    case WM_DRAWITEM: {
        auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        COLORREF color = (dis->CtlID == ID_BTN_COLOR_CN)
                             ? s_state.config.colorChinese
                             : s_state.config.colorEnglish;
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dis->hDC, &dis->rcItem, brush);
        DeleteObject(brush);
        DrawEdge(dis->hDC, &dis->rcItem, EDGE_SUNKEN, BF_RECT);
        if (dis->itemState & ODS_FOCUS)
            DrawFocusRect(dis->hDC, &dis->rcItem);
        return TRUE;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        s_hwnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

void ShowSettingsDialog(HINSTANCE hInstance, HWND owner,
                        const Config& config, bool autoStart,
                        SettingsApplyCallback onApply, void* userData) {
    if (s_hwnd) {
        SetForegroundWindow(s_hwnd);
        return;
    }

    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    (void)owner; // 见下方 CreateWindowEx 的注释
    s_state.config   = config;
    s_state.autoStart = autoStart;
    s_state.onApply  = onApply;
    s_state.userData = userData;

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = kClassName;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return;

    // 客户区 320 x 316
    RECT rc{ 0, 0, 320, 316 };
    AdjustWindowRectEx(&rc,
                       WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                       FALSE, WS_EX_DLGMODALFRAME);

    // owner 不能传消息窗口（message-only）：用它当父窗会让本窗口也变成
    // message-only 而不可见。owner 传空 + TOOLWINDOW，避免出现在任务栏。
    s_hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW,
                             kClassName, L"MushIMTip 设置",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             rc.right - rc.left, rc.bottom - rc.top,
                             nullptr, nullptr, hInstance, nullptr);
    if (s_hwnd)
        ShowWindow(s_hwnd, SW_SHOW);
}

} // namespace mushimtip
