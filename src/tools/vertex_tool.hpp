#pragma once
// Vertex tool (Shift+V), ARCHITECTURE §6 Vertex tool: press on any point and drag it
// straight away, no need to select it first (in 2D views all the points stacked there
// move together), snapped to the grid, as in Scalpellino. Everything else works like
// the Select tool in point mode (click, Cmd+click, rectangle, arrows, the box handles).
#include "tools/select_tool.hpp"

namespace mazz {

class VertexTool : public SelectTool {
public:
    const char* name() const override { return "Vertex"; }
    const char* hint() const override;
    void press(ToolEnv& env, const Pointer& p) override;
};

} // namespace mazz
