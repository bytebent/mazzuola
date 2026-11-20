#include "tools/pushpull_tool.hpp"
#include "core/model_ops.hpp"
#include "core/numbers.hpp"
#include "core/select_ops.hpp"
#include "tools/handles.hpp"
#include "ui/theme.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace mazz {

namespace {
constexpr double kEdgeOnPx = 6; // how near a face seen edge-on must be to the cursor (px)

double segDist(double px, double py, double ax, double ay, double bx, double by) {
    const double dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
    double t = l2 > 0 ? ((px - ax) * dx + (py - ay) * dy) / l2 : 0;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(px - (ax + t * dx), py - (ay + t * dy));
}

ImU32 hoverColour() { return IM_COL32(236, 92, 178, 255); }
ImU32 badColour() { return IM_COL32(230, 60, 60, 255); }

void outlineFace(ImDrawList* dl, const ViewGeom& g, const Mesh& m, int fi, ImU32 col, float w) {
    if (fi < 0 || fi >= (int)m.faces.size()) return;
    const Face& f = m.faces[fi];
    ImVec2 pts[4];
    for (int k = 0; k < f.n; ++k) {
        double x, y;
        if (!g.project(m.verts[f.v[k]], x, y)) return;
        pts[k] = ImVec2((float)x, (float)y);
    }
    dl->AddPolyline(pts, f.n, col, ImDrawFlags_Closed, w);
    if (g.is3D()) {
        // A light fill so the face reads as picked, not just its edges.
        ImU32 fill = (col & 0x00FFFFFF) | (50u << 24);
        if (f.n == 3) dl->AddTriangleFilled(pts[0], pts[1], pts[2], fill);
        else dl->AddQuadFilled(pts[0], pts[1], pts[2], pts[3], fill);
    }
}
} // namespace

const char* PushPullTool::hint() const {
    if (state_ == State::Idle) return "drag a face: push or pull it  (2D views: a face seen edge-on)  Cmd+drag: always new faces";
    return "drag: distance  Return: apply  Esc: cancel  another face: applies this one first";
}

int PushPullTool::faceAt(const Mesh& m, const ViewGeom& g, double sx, double sy) {
    if (g.is3D()) {
        const std::vector<int> hits = facesAt(m, g, sx, sy);
        return hits.empty() ? -1 : hits.front();
    }
    // 2D: faces seen edge-on (their normal across the view), near the cursor; the
    // nearest to the viewer wins.
    Vec3 ro, rd;
    g.ray(sx, sy, ro, rd);
    int best = -1;
    double bestDist = kEdgeOnPx, bestNear = -std::numeric_limits<double>::max();
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Vec3 n = faceNormal(m, fi);
        if (length(n) == 0 || std::fabs(dot(n, rd)) > 1e-6) continue;
        const Face& f = m.faces[fi];
        double d = std::numeric_limits<double>::max(), nearest = -std::numeric_limits<double>::max();
        for (int k = 0; k < f.n; ++k) {
            double ax, ay, bx, by;
            g.project(m.verts[f.v[k]], ax, ay);
            g.project(m.verts[f.v[(k + 1) % f.n]], bx, by);
            d = std::min(d, segDist(sx, sy, ax, ay, bx, by));
            nearest = std::max(nearest, -dot(m.verts[f.v[k]], rd));
        }
        if (d > kEdgeOnPx) continue;
        if (d < bestDist - 0.5 || (std::fabs(d - bestDist) <= 0.5 && nearest > bestNear)) {
            best = fi;
            bestDist = d;
            bestNear = nearest;
        }
    }
    return best;
}

void PushPullTool::begin(ToolEnv& env, int face, bool forceNew) {
    Document& doc = env.doc;
    face_ = face;
    moved_ = face;
    forceNew_ = forceNew;
    origin_ = faceCentre(doc.mesh, face);
    normal_ = faceNormal(doc.mesh, face);
    distance_ = 0;
    valid_ = true;
    kind_ = (int)pushKind(doc.mesh, face, forceNew);
    doc.beginLive();
}

void PushPullTool::refresh(ToolEnv& env) {
    Document& doc = env.doc;
    if (!doc.liveActive()) doc.beginLive();
    MeshEdit& e = doc.live();
    e.revert();
    valid_ = true;
    moved_ = face_;
    if (distance_ != 0) {
        int moved = -1;
        valid_ = pushPull(e, face_, distance_, forceNew_, &moved);
        if (valid_) moved_ = moved;
        else e.revert(); // shown as it was, outlined in red
    }
    env.touch();
}

void PushPullTool::setDistance(ToolEnv& env, double d) {
    if (state_ == State::Idle) return;
    distance_ = clean(d);
    refresh(env);
}

void PushPullTool::press(ToolEnv& env, const Pointer& p) {
    Document& doc = env.doc;
    const int at = faceAt(doc.mesh, p.g, p.sx, p.sy);
    if (state_ == State::Pending) {
        if (at < 0) return;
        // The face being pushed: carry on with this push.
        if (at == moved_ || (valid_ && at == face_ && distance_ == 0)) {
            state_ = State::Dragging;
            view_ = p.view;
            startDistance_ = distance_;
            // Measure along the line through the face where it is now.
            if (!lineParam(p.g, origin_, normal_, p.sx, p.sy, startT_)) state_ = State::Pending;
            return;
        }
        // Another face: the pending push goes in first.
        if (!apply(env)) cancel(env);
    }
    const int face = faceAt(doc.mesh, p.g, p.sx, p.sy);
    if (face < 0) return;
    double t;
    const Vec3 n = faceNormal(doc.mesh, face), c = faceCentre(doc.mesh, face);
    if (!lineParam(p.g, c, n, p.sx, p.sy, t)) return; // pointing straight at the viewer
    begin(env, face, p.cmd);
    state_ = State::Dragging;
    view_ = p.view;
    startDistance_ = 0;
    startT_ = t;
    hover_ = -1;
    env.touch();
}

void PushPullTool::drag(ToolEnv& env, const Pointer& p) {
    if (state_ != State::Dragging || p.view != view_) return;
    double t;
    if (!lineParam(p.g, origin_, normal_, p.sx, p.sy, t)) return;
    const double d = env.grid.snapValue(startDistance_ + (t - startT_));
    if (clean(d) == distance_) return;
    distance_ = clean(d);
    refresh(env);
}

void PushPullTool::release(ToolEnv& env, const Pointer&) {
    if (state_ != State::Dragging) return;
    view_ = -1;
    // Nothing pushed: nothing pending.
    if (distance_ == 0) {
        cancel(env);
        return;
    }
    state_ = State::Pending;
    env.touch();
}

void PushPullTool::hover(ToolEnv& env, const Pointer& p) {
    const int at = faceAt(env.doc.mesh, p.g, p.sx, p.sy);
    if (at != hover_ || p.view != hoverView_) {
        hover_ = at;
        hoverView_ = p.view;
        env.touch();
    }
}

bool PushPullTool::apply(ToolEnv& env) {
    if (state_ == State::Idle || state_ == State::Dragging) return false;
    if (!valid_ || distance_ == 0) return false;
    env.doc.commitLive("Push/pull");
    state_ = State::Idle;
    face_ = moved_ = -1;
    env.touch();
    return true;
}

bool PushPullTool::key(ToolEnv& env, int key, bool shift, bool cmd) {
    if (shift || cmd || state_ == State::Idle) return false;
    if (key == GLFW_KEY_ENTER) {
        apply(env);
        return true;
    }
    if (key == GLFW_KEY_ESCAPE) {
        cancel(env);
        return true;
    }
    return false;
}

void PushPullTool::cancel(ToolEnv& env) {
    if (state_ != State::Idle) env.doc.cancelLive();
    state_ = State::Idle;
    face_ = moved_ = -1;
    distance_ = 0;
    view_ = -1;
    env.touch();
}

void PushPullTool::overlay(const ToolEnv& env, const ViewGeom& g, int view, ImDrawList* dl) const {
    const Mesh& m = env.doc.mesh;
    if (state_ != State::Idle) {
        outlineFace(dl, g, m, valid_ ? moved_ : face_, valid_ ? hoverColour() : badColour(), 2.5f);
        drawDirArrow(dl, g, origin_ + normal_ * distance_, normal_, 40, valid_ ? hoverColour() : badColour(),
                     state_ == State::Dragging);
    }
    if (hover_ >= 0 && view == hoverView_ && hover_ != moved_) outlineFace(dl, g, m, hover_, hoverColour(), 1.5f);
}

void PushPullTool::properties(ToolEnv& env) {
    ImGui::SeparatorText("Push/pull");
    if (state_ == State::Idle) {
        ImGui::TextWrapped("Drag a face to push or pull it. In a 2D view, drag a face you see edge-on (a line).");
        dimWrapped("Cmd+drag: always new side faces.");
        return;
    }
    double d = distance_;
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
    if (ImGui::InputDouble("Distance (cm)", &d, 0, 0, "%g")) setDistance(env, d);
    if (ImGui::IsItemDeactivatedAfterEdit() &&
        (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
        apply(env);
    static const char* what[] = {"The faces around it stretch.", "New side faces.",
                                 "A flat shape: it becomes a solid."};
    dimWrapped(what[std::clamp(kind_, 0, 2)]);
    if (!valid_) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.2f, 0.2f, 1));
        ImGui::TextWrapped("Can't push that far: faces would turn over.");
        ImGui::PopStyleColor();
    }
    if (ImGui::Button("Apply (Return)")) apply(env);
    ImGui::SameLine();
    if (ImGui::Button("Cancel (Esc)")) cancel(env);
}

} // namespace mazz
