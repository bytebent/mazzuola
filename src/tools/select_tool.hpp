#pragma once
// Select tool (Shift+S): click picks whole pieces, Cmd+click adds/removes, a drag on
// empty space draws a rectangle; the selection box moves, resizes and rotates it
// (ARCHITECTURE §6 "Brush feel", Selection).
#include "tools/tool.hpp"

namespace mazz {

class SelectTool : public Tool {
public:
    const char* name() const override { return "Select"; }
    const char* hint() const override;
};

} // namespace mazz
