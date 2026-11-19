// The editor's side of the tool interface: the environment tools work in, switching
// tools, and the geometry of each view for picking and handles.
#include "app/editor.hpp"
#include <GLFW/glfw3.h>

namespace mazz {

ToolEnv Editor::toolEnv() { return ToolEnv{doc_, grid_, lastBox_, [this] { touch(); }}; }

void Editor::setTool(Tool* t) {
    if (t == tool_) return;
    cancelTool();
    tool_ = t;
    touch();
}

void Editor::cancelTool() {
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
    return p;
}

} // namespace mazz
