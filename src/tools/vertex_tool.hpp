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
    void drag(ToolEnv& env, const Pointer& p) override;
    void release(ToolEnv& env, const Pointer& p) override;
    void overlay(const ToolEnv& env, const ViewGeom& g, int view, ImDrawList* dl) const override;
    void cancel(ToolEnv& env) override;
    bool busy() const override { return slide_ >= 0 || SelectTool::busy(); }

private:
    // Slide (v0.2.4): Alt+drag moves a point along one of its edges, the one closest to
    // the drag's direction on screen.
    int slide_ = -1, slideTo_ = -1, slideView_ = -1;
    Vec3 slideFrom_;
    double slidePressX_ = 0, slidePressY_ = 0;
};

} // namespace mazz
