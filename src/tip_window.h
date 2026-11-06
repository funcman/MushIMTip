#pragma once

#include <Windows.h>

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
    ImeState  m_state = ImeState::English;
    bool      m_rendered = false; // DIB holds a frame for the current state

    HDC      m_memDC = nullptr;
    HBITMAP  m_dib = nullptr;
    HBITMAP  m_dibOld = nullptr; // placeholder selected into m_memDC at creation
    void*    m_bits = nullptr;
    int      m_bufW = 0;
    int      m_bufH = 0;
};

} // namespace mushimtip
