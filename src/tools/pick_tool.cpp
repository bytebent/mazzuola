#include "tools/pick_tool.hpp"
#include <algorithm>

namespace mazz {

void PickTool::press(ToolEnv& env, const Pointer& p) {
    SelectTool::press(env, p);
    // Whatever the press hit on the selection box, it is a click (or a rectangle).
    if (action_ != Action::Pending) action_ = Action::Pending;
    insideAtPress_ = false;
    grab_ = BoxGrab{};
}

void PickTool::overlay(const ToolEnv&, const ViewGeom&, int view, ImDrawList* dl) const {
    if (action_ != Action::Rect || view != view_) return;
    const ImVec2 a((float)std::min(sx0_, sx1_), (float)std::min(sy0_, sy1_));
    const ImVec2 b((float)std::max(sx0_, sx1_), (float)std::max(sy0_, sy1_));
    dl->AddRectFilled(a, b, IM_COL32(236, 92, 178, 30));
    dl->AddRect(a, b, IM_COL32(236, 92, 178, 255), 0, 0, 1.0f);
}

} // namespace mazz
