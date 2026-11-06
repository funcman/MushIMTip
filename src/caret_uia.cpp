#include "caret_uia.h"

#include <objbase.h> // WIN32_LEAN_AND_MEAN 时 windows.h 不带 COM 头
#include <uiautomation.h>

namespace mushimtip {

namespace {

IUIAutomation* g_uia = nullptr;
bool g_uiaFailed = false;

IUIAutomation* GetUia() {
    if (g_uia)
        return g_uia;
    if (g_uiaFailed)
        return nullptr;

    // CLSID_CUIAutomation8 is not required: the base CUIAutomation object
    // already exposes TextPattern2 on Windows 8+.
    HRESULT hr = CoCreateInstance(CLSID_CUIAutomation, nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_IUIAutomation,
                                  reinterpret_cast<void**>(&g_uia));
    if (FAILED(hr)) {
        g_uiaFailed = true;
        return nullptr;
    }
    return g_uia;
}

// Reads the first bounding rectangle of the caret range.
// UIA rectangles are [left, top, width, height] doubles in screen coordinates.
bool ReadCaretRect(IUIAutomationTextRange* range, RECT& rect) {
    SAFEARRAY* sa = nullptr;
    if (FAILED(range->GetBoundingRectangles(&sa)) || !sa)
        return false;

    bool ok = false;
    if (SafeArrayGetDim(sa) == 1) {
        long lb = 0, ub = -1;
        SafeArrayGetLBound(sa, 1, &lb);
        SafeArrayGetUBound(sa, 1, &ub);
        if (ub - lb + 1 >= 4) {
            double* values = nullptr;
            if (SUCCEEDED(SafeArrayAccessData(sa, reinterpret_cast<void**>(&values)))) {
                double left   = values[lb + 0];
                double top    = values[lb + 1];
                double width  = values[lb + 2];
                double height = values[lb + 3];
                SafeArrayUnaccessData(sa);
                if (height > 0) {
                    rect.left   = static_cast<long>(left);
                    rect.top    = static_cast<long>(top);
                    rect.right  = static_cast<long>(left + width);
                    rect.bottom = static_cast<long>(top + height);
                    ok = true;
                }
            }
        }
    }
    SafeArrayDestroy(sa);
    return ok;
}

} // namespace

bool QueryCaretPosUia(CaretPos& caret) {
    IUIAutomation* uia = GetUia();
    if (!uia)
        return false;

    IUIAutomationElement* focused = nullptr;
    if (FAILED(uia->GetFocusedElement(&focused)) || !focused)
        return false;

    // Try TextPattern2 directly; fall back to TextPattern and upgrade
    // (some providers only register the base pattern id).
    IUIAutomationTextPattern2* pattern = nullptr;
    if (FAILED(focused->GetCurrentPatternAs(UIA_TextPattern2Id,
                                            IID_IUIAutomationTextPattern2,
                                            reinterpret_cast<void**>(&pattern))) ||
        !pattern) {
        IUIAutomationTextPattern* base = nullptr;
        if (SUCCEEDED(focused->GetCurrentPatternAs(UIA_TextPatternId,
                                                   IID_IUIAutomationTextPattern,
                                                   reinterpret_cast<void**>(&base))) &&
            base) {
            base->QueryInterface(IID_IUIAutomationTextPattern2,
                                 reinterpret_cast<void**>(&pattern));
            base->Release();
        }
    }
    focused->Release();
    if (!pattern)
        return false;

    BOOL isActive = FALSE;
    IUIAutomationTextRange* range = nullptr;
    HRESULT hr = pattern->GetCaretRange(&isActive, &range);
    pattern->Release();
    if (FAILED(hr) || !isActive || !range) {
        if (range)
            range->Release();
        return false;
    }

    RECT rect{};
    bool ok = ReadCaretRect(range, rect);
    range->Release();
    if (!ok)
        return false;

    caret.valid    = true;
    caret.anchor.x = rect.left;
    caret.anchor.y = rect.bottom;
    caret.height   = rect.bottom - rect.top;
    return true;
}

} // namespace mushimtip
