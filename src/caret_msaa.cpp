#include "caret_msaa.h"

#include <oleacc.h>

namespace mushimtip {

namespace {

// Window to query: prefer the focus window, then the caret owner.
bool QueryCaretOwner(HWND& hwnd) {
    GUITHREADINFO gti{};
    gti.cbSize = sizeof(gti);

    HWND hwndForeground = GetForegroundWindow();
    if (!hwndForeground)
        return false;
    DWORD threadId = GetWindowThreadProcessId(hwndForeground, nullptr);
    if (threadId == 0 || !GetGUIThreadInfo(threadId, &gti))
        return false;

    hwnd = gti.hwndFocus ? gti.hwndFocus : gti.hwndCaret;
    return hwnd != nullptr;
}

// Reads the caret's screen rectangle via the OBJID_CARET accessible object.
bool ReadCaretLocation(HWND hwnd, RECT& rect) {
    IAccessible* acc = nullptr;
    if (FAILED(AccessibleObjectFromWindow(hwnd, static_cast<DWORD>(OBJID_CARET),
                                          IID_IAccessible,
                                          reinterpret_cast<void**>(&acc))) ||
        !acc)
        return false;

    VARIANT vtSelf;
    vtSelf.vt   = VT_I4;
    vtSelf.lVal = CHILDID_SELF;

    long x = 0, y = 0, w = 0, h = 0;
    HRESULT hr = acc->accLocation(&x, &y, &w, &h, vtSelf);
    acc->Release();

    if (FAILED(hr) || w < 0 || h <= 0)
        return false;

    rect.left   = x;
    rect.top    = y;
    rect.right  = x + w;
    rect.bottom = y + h;
    return true;
}

} // namespace

bool QueryCaretPosMsaa(CaretPos& caret) {
    HWND hwnd = nullptr;
    if (!QueryCaretOwner(hwnd))
        return false;

    RECT rect{};
    if (!ReadCaretLocation(hwnd, rect))
        return false;

    caret.valid    = true;
    caret.anchor.x = rect.left;
    caret.anchor.y = rect.bottom;
    caret.height   = rect.bottom - rect.top;
    return true;
}

} // namespace mushimtip
