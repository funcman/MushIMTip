#pragma once

#include <Windows.h>

#include "config.h"
#include "ime.h"

namespace mushimtip {

// Small always-on-top, click-through layered window showing the current IME
// state ("中" / "英") next to the text caret. Rendered with per-pixel alpha:
// the rounded background is semi-transparent while text and border stay
// fully opaque.
class TipWindow {
public:
    TipWindow() = default;
    ~TipWindow();

    TipWindow(const TipWindow&) = delete;
    TipWindow& operator=(const TipWindow&) = delete;

    bool Create(HINSTANCE hInstance);
    void Destroy();

    // 应用外观配置（颜色 / 不透明度 / 缩放 / 是否跟随光标高度），立即重绘。
    void ApplyConfig(const Config& config);

    // Shows/moves the tip next to the caret; re-renders only when the state
    // or the size changed.
    void ShowState(ImeState state, const CaretPos& caret);
    void Hide();

    bool IsCreated() const { return m_hwnd != nullptr; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    bool EnsureBuffers(int w, int h); // (re)allocates the DIB + memory DC
    void Render();                    // paints into the DIB and updates the window
    void RecreateFont();
    int  Scale(int value) const { return MulDiv(value, static_cast<int>(m_dpi), 96); }

    HINSTANCE m_hInstance = nullptr;
    HWND      m_hwnd = nullptr;
    UINT      m_dpi = 96;
    HFONT     m_font = nullptr;
    int       m_fontH = 0;     // 当前字体的像素高度
    ImeState  m_state = ImeState::English;
    bool      m_rendered = false; // DIB holds a frame for the current state

    // 外观配置（来自 Config，ApplyConfig 写入）
    COLORREF m_colorChinese    = RGB(0xE5, 0x39, 0x35);
    COLORREF m_colorEnglish    = RGB(0x1E, 0x90, 0xFF);
    int      m_alpha           = 180;
    int      m_scalePct        = 100;
    int      m_maxHeightPx     = 24; // 跟随光标高度时的高度上限（96 DPI 像素）
    bool     m_sizeFollowCaret = true;

    HDC      m_memDC = nullptr;
    HBITMAP  m_dib = nullptr;
    HBITMAP  m_dibOld = nullptr; // placeholder selected into m_memDC at creation
    void*    m_bits = nullptr;
    int      m_bufW = 0;
    int      m_bufH = 0;
};

} // namespace mushimtip
