#pragma once

#include "ime.h"

namespace mushimtip {

// Third caret detection channel: MSAA caret object (OBJID_CARET) of the
// focused window. Covers applications that expose accessibility via MSAA
// but implement neither the system caret nor UIA TextPattern2.
// Requires COM to be initialized on the calling thread.
bool QueryCaretPosMsaa(CaretPos& caret);

} // namespace mushimtip
