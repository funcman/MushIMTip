#pragma once

#include "ime.h"

namespace mushimtip {

// Fallback caret detection via UI Automation TextPattern2.
// Covers controls that render their own caret instead of using the system
// caret (Chrome/Edge/Electron, modern Notepad, WPF, ...).
// Requires COM to be initialized on the calling thread.
bool QueryCaretPosUia(CaretPos& caret);

} // namespace mushimtip
