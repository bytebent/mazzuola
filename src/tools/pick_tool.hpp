#pragma once
// Selecting inside tools that work on the selection (Extrude, Inset): the Select tool's
// clicks, Cmd+clicks, rectangles and edge loops, but nothing that moves, resizes or
// turns the selection (no box handles, rings or arrows).
#include "tools/select_tool.hpp"

namespace mazz {

class PickTool : public SelectTool {
public:
    void press(ToolEnv& env, const Pointer& p) override;
    void overlay(const ToolEnv& env, const ViewGeom& g, int view, ImDrawList* dl) const override;
};

} // namespace mazz
