#include "tools/prim_tool.hpp"
#include "core/numbers.hpp"
#include "core/transform.hpp"
#include "render/view_draw.hpp" // selection colour
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace mazz {

namespace {
ImU32 previewColour() {
    const Rgba c = viewcol::kSelection;
    return IM_COL32(c.r, c.g, c.b, 255);
}
} // namespace

const char* PrimTool::hint() const {
    return "drag in a 2D view: draw a box  handles: adjust  Return: create  Esc: cancel";
}

void PrimTool::press(ToolEnv& env, const Pointer& p) {
    if (p.g.is3D()) return; // drawn in the 2D views
    double u, v;
    p.g.toPlane(p.sx, p.sy, u, v);
    view_ = p.view;
    u0_ = u;
    v0_ = v;
    if (box_) {
        grab_ = hitBox2D(p.g, box_->mn, box_->mx, p.sx, p.sy, false);
        if (grab_.kind == BoxGrab::Handle || grab_.kind == BoxGrab::Inside) {
            mode_ = Mode::Editing;
            start_ = *box_;
            return;
        }
    }
    // A new rectangle: its depth comes from the last shape made or selected.
    mode_ = Mode::Drawing;
    box_.reset();
    const OrthoAxes ax = p.g.axes();
    const int depth = 3 - ax.u - ax.v;
    anchor_ = p.g.ortho.planeToWorld(env.grid.snapValue(u), env.grid.snapValue(v));
    anchor_[depth] = 0;
    env.touch();
}

void PrimTool::drag(ToolEnv& env, const Pointer& p) {
    if (mode_ == Mode::Idle || p.view != view_) return;
    double u, v;
    p.g.toPlane(p.sx, p.sy, u, v);
    if (mode_ == Mode::Drawing) {
        const OrthoAxes ax = p.g.axes();
        const int depth = 3 - ax.u - ax.v;
        const Vec3 cur = p.g.ortho.planeToWorld(env.grid.snapValue(u), env.grid.snapValue(v));
        Box b;
        for (int a : {ax.u, ax.v}) {
            b.mn[a] = clean(std::min(anchor_[a], cur[a]));
            b.mx[a] = clean(std::max(anchor_[a], cur[a]));
        }
        b.mn[depth] = env.lastBox.mn[depth];
        b.mx[depth] = env.lastBox.mx[depth];
        if (b.mx[ax.u] > b.mn[ax.u] && b.mx[ax.v] > b.mn[ax.v]) box_ = b;
        else box_.reset();
    } else {
        Box b = start_;
        if (grab_.kind == BoxGrab::Handle) {
            resizeBox(p.g, env.grid, start_.mn, start_.mx, grab_.su, grab_.sv, u - u0_, v - v0_, b.mn, b.mx);
        } else {
            const Vec3 d = moveOffset(p.g, env.grid, u - u0_, v - v0_);
            b.mn = start_.mn + d;
            b.mx = start_.mx + d;
        }
        b.mn = clean(b.mn);
        b.mx = clean(b.mx);
        box_ = b;
    }
    env.touch();
}

void PrimTool::release(ToolEnv& env, const Pointer&) {
    mode_ = Mode::Idle;
    view_ = -1;
    env.touch();
}

bool PrimTool::key(ToolEnv& env, int key, bool shift, bool cmd) {
    if (shift || cmd) return false;
    if (key == GLFW_KEY_ENTER) return create(env);
    if (key == GLFW_KEY_ESCAPE && busy()) {
        cancel(env);
        return true;
    }
    return false;
}

bool PrimTool::create(ToolEnv& env) {
    if (!box_) return false;
    const Box b = *box_;
    env.doc.apply("New box", [&](MeshEdit& e, Selection& s) {
        Mesh box;
        addBox(box, b.mn, b.mx);
        const int base = (int)e.mesh().verts.size();
        for (const Vec3& v : box.verts) e.addVert(v);
        s = Selection();
        s.mode = SelectMode::Piece;
        for (Face f : box.faces) {
            for (int k = 0; k < f.n; ++k) f.v[k] += base;
            s.faces.insert(e.addFace(f));
        }
        return true;
    });
    env.lastBox.mn = b.mn;
    env.lastBox.mx = b.mx;
    env.lastBox.valid = true;
    box_.reset();
    mode_ = Mode::Idle;
    env.touch();
    return true;
}

void PrimTool::cancel(ToolEnv& env) {
    if (!busy()) return;
    box_.reset();
    mode_ = Mode::Idle;
    view_ = -1;
    env.touch();
}

void PrimTool::overlay(const ToolEnv&, const ViewGeom& g, int, ImDrawList* dl) const {
    if (!box_) return;
    if (g.is3D()) drawBox3D(dl, g, box_->mn, box_->mx, previewColour());
    else drawBox2D(dl, g, box_->mn, box_->mx, previewColour(), true, true);
}

void PrimTool::properties(ToolEnv& env) {
    ImGui::SeparatorText("Primitive");
    // v0.1.0 has the box; the round shapes follow in v0.1.1.
    ImGui::TextUnformatted("Shape");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##shape", "Box")) {
        ImGui::Selectable("Box", true);
        ImGui::BeginDisabled();
        for (const char* s : {"Plane", "Cylinder", "Cone", "Tube", "Torus", "Sphere", "Geosphere"}) ImGui::Selectable(s);
        ImGui::EndDisabled();
        ImGui::EndCombo();
    }
    if (!box_) {
        ImGui::TextDisabled("Drag a rectangle in a 2D view.");
        return;
    }
    // Size typed in cm; the box grows from its lowest corner.
    static const char* axisName[3] = {"Width (x)", "Depth (y)", "Height (z)"};
    for (int a = 0; a < 3; ++a) {
        double size = box_->mx[a] - box_->mn[a];
        ImGui::PushID(a);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
        if (ImGui::InputDouble(axisName[a], &size, 0, 0, "%.3g cm") && size > 0) {
            box_->mx[a] = clean(box_->mn[a] + clean(size));
            env.touch();
        }
        ImGui::PopID();
    }
    if (ImGui::Button("Create (Return)")) create(env);
    ImGui::SameLine();
    if (ImGui::Button("Cancel (Esc)")) cancel(env);
}

} // namespace mazz
