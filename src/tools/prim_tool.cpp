#include "tools/prim_tool.hpp"
#include "core/numbers.hpp"
#include "core/transform.hpp"
#include "render/view_draw.hpp" // selection colour
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <random>
#include <set>

namespace mazz {

namespace {
bool nearV0(const Vec3& a, const Vec3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

ImU32 previewColour() {
    const Rgba c = viewcol::kSelection;
    return IM_COL32(c.r, c.g, c.b, 255);
}
} // namespace

unsigned PrimTool::newSeed() {
    // A fresh seed each time (not the same series every launch).
    static std::random_device rd;
    return rd() % 1000000;
}

void PrimTool::setShape(PrimKind k) {
    if (k == params_.kind) return;
    saved_[(int)params_.kind] = params_;
    params_ = saved_[(int)k];
    params_.kind = k;
}

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
    // A new rectangle: its depth comes from the last shape made or selected; round
    // shapes stand along the view's depth axis.
    mode_ = Mode::Drawing;
    box_.reset();
    const OrthoAxes ax = p.g.axes();
    const int depth = 3 - ax.u - ax.v;
    axis_ = depth;
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
        // Depth from the last shape; a flat one (a plane) gives no depth, so then the
        // default 64 cm from the same level.
        b.mn[depth] = env.lastBox.mn[depth];
        b.mx[depth] = env.lastBox.mx[depth] > env.lastBox.mn[depth] ? env.lastBox.mx[depth] : b.mn[depth] + 64;
        if (b.mx[ax.u] > b.mn[ax.u] && b.mx[ax.v] > b.mn[ax.v]) box_ = b;
        else box_.reset();
    } else {
        Box b = start_;
        if (grab_.kind == BoxGrab::Handle) {
            resizeBox(p.g, env.grid, start_.mn, start_.mx, grab_.su, grab_.sv, u - u0_, v - v0_, b.mn, b.mx);
        } else {
            const Vec3 d = moveOffset(p.g, env.grid, start_.mn, u - u0_, v - v0_);
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
    const std::string what = std::string("New ") + primKindName(params_.kind);
    env.doc.apply(what, [&](MeshEdit& e, Selection& s) {
        const Mesh box = makePrimitive(params_, b.mn, b.mx, axis_);
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

const Mesh& PrimTool::previewMesh() const {
    const bool same = previewValid_ && previewAxis_ == axis_ && nearV0(previewMn_, box_->mn) && nearV0(previewMx_, box_->mx) &&
                      previewParams_ == params_;
    if (!same) {
        preview_ = makePrimitive(params_, box_->mn, box_->mx, axis_);
        previewValid_ = true;
        previewAxis_ = axis_;
        previewMn_ = box_->mn;
        previewMx_ = box_->mx;
        previewParams_ = params_;
        std::set<Edge> edges;
        for (const Face& f : preview_.faces)
            for (int k = 0; k < f.n; ++k) edges.insert(makeEdge(f.v[k], f.v[(k + 1) % f.n]));
        previewEdges_.assign(edges.begin(), edges.end());
    }
    return preview_;
}

int PrimTool::previewFaces() const { return box_ ? (int)previewMesh().faces.size() : 0; }

void PrimTool::overlay(const ToolEnv&, const ViewGeom& g, int, ImDrawList* dl) const {
    if (!box_) return;
    // The shape itself, as lines (each edge once), then the box and its handles.
    const Mesh& m = previewMesh();
    const ImU32 col = previewColour();
    std::vector<ImVec2> pts(m.verts.size());
    std::vector<char> seen(m.verts.size());
    for (size_t i = 0; i < m.verts.size(); ++i) {
        double x, y;
        seen[i] = g.project(m.verts[i], x, y);
        pts[i] = ImVec2((float)x, (float)y);
    }
    for (const Edge& e : previewEdges_)
        if (seen[e.first] && seen[e.second]) dl->AddLine(pts[e.first], pts[e.second], col, 1.0f);
    if (g.is3D()) drawBox3D(dl, g, box_->mn, box_->mx, IM_COL32(150, 150, 160, 160));
    else drawBox2D(dl, g, box_->mn, box_->mx, col, true, true);
}

void PrimTool::properties(ToolEnv& env) {
    ImGui::SeparatorText("Primitive");
    ImGui::TextUnformatted("Shape");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##shape", primKindName(params_.kind))) {
        for (int k = 0; k < kPrimKindCount; ++k)
            if (ImGui::Selectable(primKindName((PrimKind)k), (int)params_.kind == k)) {
                setShape((PrimKind)k);
                env.touch();
            }
        ImGui::EndCombo();
    }
    // The shape's settings (remembered between uses).
    bool changed = false;
    const float w = ImGui::GetContentRegionAvail().x * 0.45f;
    auto intField = [&](const char* label, int& v) {
        ImGui::SetNextItemWidth(w);
        if (ImGui::InputInt(label, &v)) changed = true;
    };
    auto pctField = [&](const char* label, double& v) {
        ImGui::SetNextItemWidth(w);
        if (ImGui::InputDouble(label, &v, 5, 10, "%.0f %%")) changed = true;
    };
    switch (params_.kind) {
    case PrimKind::Box:
        ImGui::BeginDisabled(params_.roundSegments > 0); // a rounded box has plain flat sides
        intField("Segments x", params_.segX);
        intField("Segments y", params_.segY);
        intField("Segments z", params_.segZ);
        ImGui::EndDisabled();
        intField("Rounded edges", params_.roundSegments);
        if (params_.roundSegments > 0) pctField("Rounding", params_.roundSize);
        break;
    case PrimKind::Plane:
        intField("Rows", params_.rows);
        intField("Columns", params_.cols);
        if (ImGui::Checkbox("Flip diagonals", &params_.flipDiagonals)) changed = true;
        break;
    case PrimKind::Cylinder:
    case PrimKind::Cone:
        intField("Sides", params_.sides);
        intField("Segments", params_.segments);
        intField("Cap rings", params_.capRings);
        if (params_.kind == PrimKind::Cylinder) {
            intField("Rounded edges", params_.roundSegments);
            if (params_.roundSegments > 0) pctField("Rounding", params_.roundSize);
        }
        break;
    case PrimKind::Tube:
        intField("Sides", params_.sides);
        intField("Segments", params_.segments);
        pctField("Hole", params_.inner);
        break;
    case PrimKind::Torus:
        intField("Sides", params_.sides);
        intField("Tube sides", params_.tubeSides);
        pctField("Tube width", params_.tubeWidth);
        break;
    case PrimKind::Sphere:
        intField("Sides", params_.sides);
        intField("Stacks", params_.stacks);
        break;
    case PrimKind::Geosphere:
        intField("Iterations", params_.iterations);
        break;
    case PrimKind::Rock: {
        RockParams& r = params_.rock;
        intField("Faces", r.faces);
        pctField("Roughness", r.roughness);
        if (ImGui::Checkbox("Flat bottom", &r.flatBottom)) changed = true;
        if (r.flatBottom) pctField("Bottom height", r.bottomHeight);
        if (ImGui::Checkbox("Join into quads", &r.joinQuads)) changed = true;
        int seed = (int)r.seed;
        ImGui::SetNextItemWidth(w);
        if (ImGui::InputInt("Seed", &seed)) {
            r.seed = (unsigned)std::max(0, seed);
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("New seed")) {
            r.seed = newSeed();
            changed = true;
        }
        break;
    }
    }
    if (changed) {
        params_ = clampParams(params_);
        env.touch();
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
