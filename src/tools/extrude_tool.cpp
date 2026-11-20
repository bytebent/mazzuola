#include "tools/extrude_tool.hpp"
#include "core/numbers.hpp"
#include "core/ops.hpp"
#include "core/select_ops.hpp"
#include "tools/handles.hpp"
#include <cmath>

namespace mazz {

namespace {
constexpr float kArrowPx = 48;
constexpr double kGrabPx = 10;
const char* kAxisNames[3] = {"X", "Y", "Z"};

Vec3 axisDir(int a) {
    Vec3 d{0, 0, 0};
    (a == 0 ? d.x : a == 1 ? d.y : d.z) = 1;
    return d;
}

double segDist(double px, double py, double ax, double ay, double bx, double by) {
    const double dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
    double t = l2 > 0 ? ((px - ax) * dx + (py - ay) * dy) / l2 : 0;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return std::hypot(px - (ax + t * dx), py - (ay + t * dy));
}
} // namespace

const char* ExtrudeTool::hint() const {
    if (!armed_) return "select faces, or open edges (edge mode)  click / rectangle: select";
    return "drag the arrow or type the distance  Return: apply  Esc: cancel  click: select other faces";
}

bool ExtrudeTool::suits(const Mesh& m, const Selection& s) const {
    if (s.mode == SelectMode::Edge) return !openEdges(m, s.edges).empty();
    if (s.mode == SelectMode::Face || s.mode == SelectMode::Piece) return !s.faces.empty();
    return false;
}

bool ExtrudeTool::compute(MeshEdit& e, const Selection& source, Selection& result) {
    // The arrow, from the model as it was (fixed while dragging).
    arrowOk_ = arrowFor(e.mesh(), source, arrowOrigin_, arrowDir_);
    if (edges(source)) {
        std::set<Edge> far;
        if (!extrudeEdges(e, source.edges, axisDir(params_.axis) * params_.distance, &far)) return false;
        result = Selection();
        result.mode = SelectMode::Edge;
        result.edges = far;
        return true;
    }
    std::set<int> ends;
    if (!extrudeFaces(e, source.faces, params_, &ends)) return false;
    result = Selection();
    result.mode = SelectMode::Face;
    result.faces = ends;
    return true;
}

bool ExtrudeTool::arrow(const Mesh&, Vec3& origin, Vec3& dir) const {
    if (!armed_ || !arrowOk_) return false;
    origin = arrowOrigin_;
    dir = arrowDir_;
    return true;
}

bool ExtrudeTool::arrowFor(const Mesh& m, const Selection& s, Vec3& origin, Vec3& dir) const {
    Vec3 mn, mx;
    if (!selectionBounds(m, s, mn, mx)) return false;
    origin = (mn + mx) * 0.5;
    if (edges(s) || params_.straight) dir = axisDir(params_.axis);
    else {
        dir = selectionNormal(m, s);
        const double l = length(dir);
        if (l < 1e-6) return false;
        dir = dir * (1.0 / l);
    }
    return true;
}

bool ExtrudeTool::pressHandle(ToolEnv& env, const Pointer& p) {
    Vec3 o, d;
    if (!arrow(env.doc.mesh, o, d)) return false;
    const Vec3 at = o + d * params_.distance;
    double ax, ay, tx, ty;
    if (!p.g.project(at, ax, ay) || !dirArrowTip(p.g, at, d, kArrowPx, tx, ty)) return false;
    if (segDist(p.sx, p.sy, ax, ay, tx, ty) > kGrabPx) return false;
    double t;
    if (!lineParam(p.g, o, d, p.sx, p.sy, t)) return false;
    dragView_ = p.view;
    dragOrigin_ = o;
    dragDir_ = d;
    startT_ = t;
    startDistance_ = params_.distance;
    return true;
}

void ExtrudeTool::dragHandle(ToolEnv& env, const Pointer& p) {
    if (p.view != dragView_) return;
    double t;
    if (!lineParam(p.g, dragOrigin_, dragDir_, p.sx, p.sy, t)) return;
    const double d = clean(env.grid.snapValue(startDistance_ + (t - startT_)));
    if (d == params_.distance) return;
    params_.distance = d;
    refresh(env);
}

void ExtrudeTool::drawHandle(const ToolEnv& env, const ViewGeom& g, int, ImDrawList* dl) const {
    Vec3 o, d;
    if (!arrow(env.doc.mesh, o, d)) return;
    const ImU32 col = valid_ ? IM_COL32(236, 92, 178, 255) : IM_COL32(230, 60, 60, 255);
    drawDirArrow(dl, g, o + d * params_.distance, d, kArrowPx, col, dragging_);
}

void ExtrudeTool::properties(ToolEnv& env) {
    ImGui::SeparatorText("Extrude");
    const Document& doc = env.doc;
    const bool edgeMode = (armed_ ? source_ : doc.sel).mode == SelectMode::Edge;
    if (!armed_ && !suits(doc.mesh, doc.sel)) {
        ImGui::TextWrapped("Select faces to extrude, or open edges (edge mode: edges with a face on one side only).");
    }
    bool ch = false;
    const float fieldW = ImGui::GetFontSize() * 7;
    ImGui::SetNextItemWidth(fieldW);
    double d = params_.distance;
    if (ImGui::InputDouble("Distance (cm)", &d, 0, 0, "%g")) {
        params_.distance = clean(d);
        ch = true;
    }
    applyOnReturn(env);
    if (!edgeMode) {
        if (ImGui::RadioButton("Region", !params_.each)) params_.each = false, ch = true;
        ImGui::SameLine();
        if (ImGui::RadioButton("Each face", params_.each)) params_.each = true, ch = true;
        if (ImGui::RadioButton("Along normals", !params_.straight)) params_.straight = false, ch = true;
        ImGui::SameLine();
        if (ImGui::RadioButton("Straight", params_.straight)) params_.straight = true, ch = true;
    } else {
        dimWrapped("Edges extrude straight.");
    }
    if (edgeMode || params_.straight) {
        for (int a = 0; a < 3; ++a) {
            if (a) ImGui::SameLine();
            ImGui::PushID(a);
            if (ImGui::RadioButton(kAxisNames[a], params_.axis == a)) params_.axis = a, ch = true;
            ImGui::PopID();
        }
    }
    if (!edgeMode && ImGui::Checkbox("Keep original", &params_.keepOriginal)) ch = true;
    if (!edgeMode) dimWrapped("Keep original: a flat shape becomes a solid.");
    applyButtons(env, "Can't extrude that way (the new faces would be flat).");
    if (ch) changed(env);
}

} // namespace mazz
