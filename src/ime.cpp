#include "ime.h"

#include <imm.h>

#include "caret_uia.h"
#include "caret_msaa.h"

// WM_IME_CONTROL query codes. Recent Windows SDKs dropped these legacy
// constants from imm.h, but the IME window messages still use them.
#ifndef IMC_GETCONVERSIONMODE
#define IMC_GETCONVERSIONMODE 0x0001
#endif
#ifndef IMC_GETOPENSTATUS
#define IMC_GETOPENSTATUS 0x0005
#endif

namespace mushimtip {

namespace {

// Fills gti with the GUI thread info of the thread owning the foreground window.
bool QueryForegroundGuiInfo(GUITHREADINFO& gti) {
    gti = {};
    gti.cbSize = sizeof(gti);

    HWND hwndForeground = GetForegroundWindow();
    if (!hwndForeground)
        return false;

    DWORD threadId = GetWindowThreadProcessId(hwndForeground, nullptr);
    if (threadId == 0)
        return false;

    return GetGUIThreadInfo(threadId, &gti) != FALSE;
}

// Polls the system caret of the foreground thread. The GUI_CARETBLINKING
// flag toggles with the caret's blink phase, so an "off" phase is treated
// as still visible for a grace period (one blink cycle + slack) instead of
// hiding the tip on every other poll.
CaretPos QueryCaretPosSystem() {
    CaretPos result;

    GUITHREADINFO gti;
    if (!QueryForegroundGuiInfo(gti))
        return result;
    if (!gti.hwndCaret)
        return result;

    static DWORD s_lastBlinkSeen = 0;
    static HWND  s_lastCaret     = nullptr;
    const DWORD now = GetTickCount();

    if (gti.flags & GUI_CARETBLINKING) {
        s_lastBlinkSeen = now;
        s_lastCaret     = gti.hwndCaret;
    } else {
        const DWORD grace = GetCaretBlinkTime() + 200;
        if (s_lastCaret != gti.hwndCaret || now - s_lastBlinkSeen > grace)
            return result;
    }

    // rcCaret is relative to the client area of hwndCaret.
    POINT anchor{ gti.rcCaret.left, gti.rcCaret.bottom };
    if (!ClientToScreen(gti.hwndCaret, &anchor))
        return result;

    result.valid  = true;
    result.anchor = anchor;
    result.height = gti.rcCaret.bottom - gti.rcCaret.top;
    return result;
}

} // namespace

CaretPos QueryCaretPos() {
    CaretPos caret = QueryCaretPosSystem();
    if (!caret.valid)
        QueryCaretPosUia(caret);
    if (!caret.valid)
        QueryCaretPosMsaa(caret);
    return caret;
}

ImeState QueryImeState() {
    HWND hwndTarget = nullptr;

    GUITHREADINFO gti;
    if (QueryForegroundGuiInfo(gti))
        hwndTarget = gti.hwndFocus ? gti.hwndFocus : gti.hwndActive;
    if (!hwndTarget)
        hwndTarget = GetForegroundWindow();
    if (!hwndTarget)
        return ImeState::English;

    // The default IME window handles WM_IME_CONTROL queries for the thread.
    // A null window means the keyboard layout has no IME (e.g. plain US).
    HWND hwndIme = ImmGetDefaultIMEWnd(hwndTarget);
    if (!hwndIme)
        return ImeState::English;

    if (!SendMessageW(hwndIme, WM_IME_CONTROL, IMC_GETOPENSTATUS, 0))
        return ImeState::English;

    LRESULT mode = SendMessageW(hwndIme, WM_IME_CONTROL, IMC_GETCONVERSIONMODE, 0);
    return (mode & IME_CMODE_NATIVE) ? ImeState::Chinese : ImeState::English;
}

} // namespace mushimtip
