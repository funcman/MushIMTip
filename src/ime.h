#pragma once

#include <Windows.h>

namespace mushimtip {

// IME conversion state of the focused control in the foreground thread.
enum class ImeState {
    English, // IME closed / alphanumeric mode, or the layout has no IME at all
    Chinese, // IME open and in a native (e.g. Chinese) conversion mode
};

// Where the text caret of the foreground thread currently is.
struct CaretPos {
    bool  valid = false; // a caret exists and is currently visible (blinking on)
    POINT anchor{};      // screen coordinates of the caret's bottom-left corner
    int   height = 0;    // caret height in pixels
};

// Polls the foreground thread's GUI info. Works across processes as long as
// the target window belongs to the same desktop and is not elevated above us.
CaretPos QueryCaretPos();
ImeState QueryImeState();

} // namespace mushimtip
