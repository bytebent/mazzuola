#include "tools/vertex_tool.hpp"
#include "core/select_ops.hpp"

namespace mazz {

const char* VertexTool::hint() const {
    return "drag a point: move it  click: select  Cmd+click: add/remove  drag on empty space: rectangle  "
           "arrows: along an axis  Esc: select nothing";
}

void VertexTool::press(ToolEnv& env, const Pointer& p) {
    SelectTool::press(env, p);
    Document& doc = env.doc;
    // The arrows keep working; Cmd+click and double-click select as usual.
    if (action_ == Action::Arrow || p.cmd || p.clicks >= 2) return;
    const std::set<int> pts = pointsAt(doc.mesh, p.g, p.sx, p.sy);
    if (pts.empty()) return;
    // A point under the cursor is grabbed straight away (before the box handles). If it
    // is already selected, all the selected points move together.
    bool allSelected = doc.sel.mode == SelectMode::Vert;
    for (int v : pts) allSelected = allSelected && doc.sel.verts.count(v) > 0;
    if (!allSelected) {
        Selection s;
        s.mode = SelectMode::Vert;
        s.verts = pts;
        doc.select(s);
    }
    selectionBounds(doc.mesh, doc.sel, mn0_, mx0_);
    action_ = Action::Pending;
    grab_ = BoxGrab{};
    insideAtPress_ = true; // a drag now moves them (Shift: copy)
    env.touch();
}

} // namespace mazz
