// The editor's side of the tool interface: the environment tools work in, switching
// tools, and the geometry of each view for picking and handles.
#include "app/editor.hpp"
#include <GLFW/glfw3.h>

namespace mazz {

// Re-uploads the model when its geometry changed, the selection lists when the
// selection changed, and marks the views for a redraw only then.
void Editor::refreshModelDrawing() {
    const bool meshChanged = doc_.meshVersion() != drawnMesh_;
    const bool selChanged = meshChanged || doc_.selVersion() != drawnSel_;
    if (meshChanged) {
        buildDrawData(doc_.mesh, drawData_);
        drawer_.setModel(drawData_);
        drawnMesh_ = doc_.meshVersion();
    }
    if (selChanged) {
        drawer_.setSelection(selectedTriIndices(drawData_, doc_.sel), selectedEdgeIndices(doc_.mesh, drawData_, doc_.sel));
        drawnSel_ = doc_.selVersion();
        markViewsDirty();
        // New shapes take their depth from the last shape made or selected.
        Vec3 mn, mx;
        if (!doc_.sel.empty() && selectionBounds(doc_.mesh, doc_.sel, mn, mx)) {
            lastBox_.mn = mn;
            lastBox_.mx = mx;
            lastBox_.valid = true;
        }
    }
}

ToolEnv Editor::toolEnv() { return ToolEnv{doc_, grid_, lastBox_, [this] { touch(); }}; }

void Editor::setTool(Tool* t) {
    if (t == tool_) return;
    cancelTool();
    tool_ = t;
    touch();
}

void Editor::cancelTool() {
    cancelOp();
    ToolEnv env = toolEnv();
    tool_->cancel(env);
    toolView_ = -1;
}

ViewGeom Editor::geom(int i) const {
    ViewGeom g;
    const View& v = views_[i];
    g.dir = v.dir;
    g.ortho = v.ortho;
    g.cam = v.cam3d;
    g.x0 = viewImgPos_[i].x;
    g.y0 = viewImgPos_[i].y;
    g.w = std::max(1.0f, viewImgSize_[i].x);
    g.h = std::max(1.0f, viewImgSize_[i].y);
    return g;
}

Pointer Editor::pointer(int i) const {
    Pointer p;
    p.view = i;
    p.g = geom(i);
    const ImGuiIO& io = ImGui::GetIO();
    p.sx = io.MousePos.x;
    p.sy = io.MousePos.y;
    p.cmd = io.KeySuper || io.KeyCtrl;
    p.shift = io.KeyShift;
    p.alt = io.KeyAlt;
    p.clicks = std::max(1, ImGui::GetMouseClickedCount(ImGuiMouseButton_Left));
    return p;
}

} // namespace mazz
