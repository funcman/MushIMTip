#include "tip_window.h"

namespace mushimtip {

namespace {

constexpr wchar_t kClassName[] = L"MushIMTipTipWindow";

// Design metrics at 96 DPI, scaled with the window DPI.
constexpr int kTipWidth   = 40;
constexpr int kTipHeight  = 22;
constexpr int kTipOffsetX = 8; // gap between caret and tip
constexpr int kTipOffsetY = 4;

constexpr COLORREF kColorChinese = RGB(0xE5, 0x39, 0x35); // red
constexpr COLORREF kColorEnglish = RGB(0x1E, 0x90, 0xFF); // blue

// Opacity of the rounded background (0-255); text and border stay opaque.
constexpr BYTE kBackgroundAlpha = 180;

HFONT CreateTipFont(int height13) {
    return CreateFontW(-height13, 0, 0, 0, FW_SEMIBOLD,
                       FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                       L"Microsoft YaHei UI");
}

} // namespace

TipWindow::~TipWindow() {
    Destroy();
}

bool TipWindow::Create(HINSTANCE hInstance) {
    if (m_hwnd)
        return true;
    m_hInstance = hInstance;

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &TipWindow::WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = kClassName;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    // LAYERED enables per-pixel alpha via UpdateLayeredWindow; TOOLWINDOW
    // keeps us out of Alt-Tab and NOACTIVATE/TRANSPARENT make the tip
    // purely visual.
    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT |
            WS_EX_LAYERED,
        kClassName, L"MushIMTip",
        WS_POPUP,
        0, 0, kTipWidth, kTipHeight,
        nullptr, nullptr, hInstance, this);
    if (!m_hwnd)
        return false;

    m_dpi = GetDpiForWindow(m_hwnd);
    RecreateFont();

    const int w = Scale(kTipWidth);
    const int h = Scale(kTipHeight);
    if (!EnsureBuffers(w, h))
        return false;
    Render();

    SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, w, h,
                 SWP_NOMOVE | SWP_NOACTIVATE | SWP_HIDEWINDOW);
    return true;
}

void TipWindow::Destroy() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    if (m_dib) {
        DeleteObject(m_dib);
        m_dib = nullptr;
        m_bits = nullptr;
    }
    if (m_memDC) {
        DeleteDC(m_memDC);
        m_memDC = nullptr;
    }
    if (m_font) {
        DeleteObject(m_font);
        m_font = nullptr;
    }
    m_bufW = m_bufH = 0;
}

void TipWindow::RecreateFont() {
    if (m_font)
        DeleteObject(m_font);
    m_font = CreateTipFont(Scale(13));
}

bool TipWindow::EnsureBuffers(int w, int h) {
    if (m_memDC && m_bufW == w && m_bufH == h)
        return true;

    if (!m_memDC) {
        HDC screen = GetDC(nullptr);
        m_memDC = CreateCompatibleDC(screen);
        ReleaseDC(nullptr, screen);
        if (!m_memDC)
            return false;
    }

    if (m_dib) {
        SelectObject(m_memDC, m_dibOld);
        DeleteObject(m_dib);
        m_dib = nullptr;
        m_bits = nullptr;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize        = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h; // top-down rows
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    m_dib = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &m_bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!m_dib)
        return false;

    m_dibOld = static_cast<HBITMAP>(SelectObject(m_memDC, m_dib));
    m_bufW = w;
    m_bufH = h;
    m_rendered = false;
    return true;
}

void TipWindow::Render() {
    if (!m_memDC || !m_bits || m_bufW <= 0 || m_bufH <= 0)
        return;

    // Start fully transparent; GDI only touches pixels inside the shapes it
    // draws, so anything left at RGB(0,0,0) afterwards is "outside".
    ZeroMemory(m_bits, static_cast<size_t>(m_bufW) * m_bufH * 4);

    const bool chinese = (m_state == ImeState::Chinese);
    const COLORREF bgColor = chinese ? kColorChinese : kColorEnglish;

    RECT rc{ 0, 0, m_bufW, m_bufH };
    HBRUSH brush = CreateSolidBrush(bgColor);
    HPEN   pen   = CreatePen(PS_SOLID, 1, RGB(0xFF, 0xFF, 0xFF));
    HGDIOBJ oldBrush = SelectObject(m_memDC, brush);
    HGDIOBJ oldPen   = SelectObject(m_memDC, pen);
    RoundRect(m_memDC, rc.left, rc.top, rc.right - 1, rc.bottom - 1,
              Scale(6), Scale(6));

    const wchar_t* text = chinese ? L"中" : L"英";
    SetBkMode(m_memDC, TRANSPARENT);
    SetTextColor(m_memDC, RGB(0xFF, 0xFF, 0xFF));
    HGDIOBJ oldFont = m_font ? SelectObject(m_memDC, m_font) : nullptr;
    DrawTextW(m_memDC, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (oldFont) SelectObject(m_memDC, oldFont);
    SelectObject(m_memDC, oldPen);
    SelectObject(m_memDC, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);

    // Alpha pass (GDI leaves the alpha byte at 0). UpdateLayeredWindow wants
    // premultiplied colors, so background pixels get bg*alpha/255.
    auto* px = static_cast<BYTE*>(m_bits);
    const BYTE bgR = GetRValue(bgColor);
    const BYTE bgG = GetGValue(bgColor);
    const BYTE bgB = GetBValue(bgColor);
    const size_t count = static_cast<size_t>(m_bufW) * m_bufH;
    for (size_t i = 0; i < count; ++i, px += 4) {
        const BYTE b = px[0], g = px[1], r = px[2];
        if (r == 0 && g == 0 && b == 0) {
            px[3] = 0; // outside the rounded shape
        } else if (r == bgR && g == bgG && b == bgB) {
            px[0] = static_cast<BYTE>((b * kBackgroundAlpha) / 255);
            px[1] = static_cast<BYTE>((g * kBackgroundAlpha) / 255);
            px[2] = static_cast<BYTE>((r * kBackgroundAlpha) / 255);
            px[3] = kBackgroundAlpha;
        } else {
            px[3] = 255; // text, border, and their antialiased edges
        }
    }

    POINT src{ 0, 0 };
    SIZE  size{ m_bufW, m_bufH };
    BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    HDC screen = GetDC(nullptr);
    UpdateLayeredWindow(m_hwnd, screen, nullptr, &size, m_memDC, &src,
                        0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screen);

    m_rendered = true;
}

void TipWindow::ShowState(ImeState state, const CaretPos& caret) {
    if (!m_hwnd)
        return;

    const int w = Scale(kTipWidth);
    const int h = Scale(kTipHeight);

    int x = caret.anchor.x + Scale(kTipOffsetX);
    int y = caret.anchor.y + Scale(kTipOffsetY);

    // Keep the tip inside the work area of the monitor holding the caret.
    HMONITOR monitor = MonitorFromPoint(caret.anchor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) };
    if (GetMonitorInfoW(monitor, &mi)) {
        const RECT& wa = mi.rcWork;
        if (x + w > wa.right)
            x = caret.anchor.x - w - Scale(kTipOffsetX); // flip to the left of the caret
        if (y + h > wa.bottom)
            y = caret.anchor.y - h - caret.height - Scale(kTipOffsetY); // flip above
        if (x < wa.left)  x = wa.left;
        if (y < wa.top)   y = wa.top;
    }

    const bool stateChanged = !m_rendered || m_state != state;
    m_state = state;
    if (EnsureBuffers(w, h) && stateChanged)
        Render();

    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, w, h,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void TipWindow::Hide() {
    if (m_hwnd)
        ShowWindow(m_hwnd, SW_HIDE);
}

LRESULT CALLBACK TipWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    TipWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<TipWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<TipWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self)
        return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT TipWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DPICHANGED: {
        m_dpi = LOWORD(wParam);
        RecreateFont();
        m_rendered = false; // force a re-render at the new scale
        // Apply the system-suggested rect; the polling loop will re-anchor.
        auto* suggested = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(m_hwnd, nullptr,
                     suggested->left, suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

} // namespace mushimtip
