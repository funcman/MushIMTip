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

// Reads the first bounding rectangle of a text range.
// UIA rectangles are [left, top, width, height] doubles in screen coordinates.
bool ReadFirstRect(IUIAutomationTextRange* range, RECT& rect) {
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

void ApplyRect(CaretPos& caret, const RECT& rect, bool anchorRight) {
    caret.valid    = true;
    caret.anchor.x = anchorRight ? rect.right : rect.left;
    caret.anchor.y = rect.bottom;
    caret.height   = rect.bottom - rect.top;
}

// TextPattern2 path: direct caret range (Chrome/Edge, WPF, modern Notepad).
bool QueryViaCaretRange(IUIAutomationTextPattern2* pattern, CaretPos& caret) {
    BOOL isActive = FALSE;
    IUIAutomationTextRange* range = nullptr;
    HRESULT hr = pattern->GetCaretRange(&isActive, &range);
    if (FAILED(hr) || !isActive || !range) {
        if (range)
            range->Release();
        return false;
    }

    RECT rect{};
    bool ok = ReadFirstRect(range, rect);
    range->Release();
    if (!ok)
        return false;

    ApplyRect(caret, rect, false);
    return true;
}

// TextPattern path: providers without TextPattern2 (Windows Terminal, some
// Electron controls) expose the caret as a degenerate selection range.
// Such ranges often have no bounding rectangle, so the range is expanded
// to the character at the caret and that character's rect is used instead.
bool QueryViaSelection(IUIAutomationTextPattern* pattern, CaretPos& caret) {
    IUIAutomationTextRangeArray* selection = nullptr;
    if (FAILED(pattern->GetSelection(&selection)) || !selection)
        return false;

    IUIAutomationTextRange* range = nullptr;
    int length = 0;
    selection->get_Length(&length);
    if (length > 0)
        selection->GetElement(0, &range);
    selection->Release();
    if (!range)
        return false;

    bool ok = false;
    RECT rect{};

    // Non-degenerate selection or a provider that does report caret rects.
    if (ReadFirstRect(range, rect)) {
        ApplyRect(caret, rect, false);
        ok = true;
    } else {
        // Degenerate range without geometry: expand to the character at the
        // caret; its left edge is the caret position.
        IUIAutomationTextRange* clone = nullptr;
        if (SUCCEEDED(range->Clone(&clone)) && clone) {
            if (SUCCEEDED(clone->ExpandToEnclosingUnit(TextUnit_Character)) &&
                ReadFirstRect(clone, rect)) {
                ApplyRect(caret, rect, false);
                ok = true;
            }
            clone->Release();
        }
        if (!ok) {
            // Caret may sit at the very end of the document with no character
            // to expand to: step one character back and anchor at the right
            // edge of the preceding character. Move() shifts a degenerate
            // range as a whole.
            IUIAutomationTextRange* back = nullptr;
            if (SUCCEEDED(range->Clone(&back)) && back) {
                int moved = 0;
                if (SUCCEEDED(back->Move(TextUnit_Character, -1, &moved)) && moved != 0 &&
                    SUCCEEDED(back->ExpandToEnclosingUnit(TextUnit_Character)) &&
                    ReadFirstRect(back, rect)) {
                    ApplyRect(caret, rect, true);
                    ok = true;
                }
                back->Release();
            }
        }
    }

    range->Release();
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

    IUIAutomationTextPattern* pattern = nullptr;
    HRESULT hr = focused->GetCurrentPatternAs(UIA_TextPatternId,
                                              IID_IUIAutomationTextPattern,
                                              reinterpret_cast<void**>(&pattern));
    focused->Release();
    if (FAILED(hr) || !pattern)
        return false;

    bool ok = false;

    // Preferred: TextPattern2 caret range.
    IUIAutomationTextPattern2* pattern2 = nullptr;
    if (SUCCEEDED(pattern->QueryInterface(IID_IUIAutomationTextPattern2,
                                          reinterpret_cast<void**>(&pattern2))) &&
        pattern2) {
        ok = QueryViaCaretRange(pattern2, caret);
        pattern2->Release();
    }

    // Fallback: degenerate selection (+ character expansion).
    if (!ok)
        ok = QueryViaSelection(pattern, caret);

    pattern->Release();
    return ok;
}

} // namespace mushimtip
