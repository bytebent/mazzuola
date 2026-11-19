#pragma once
// Primitives tool (Shift+B): drag a rectangle in a 2D view, adjust the preview with the
// brush handles, Return creates it, Esc cancels (ARCHITECTURE §6 Primitives).
#include "tools/tool.hpp"

namespace mazz {

class PrimTool : public Tool {
public:
    const char* name() const override { return "Primitives"; }
    const char* hint() const override;
};

} // namespace mazz
