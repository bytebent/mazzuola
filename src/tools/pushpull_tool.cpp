#include "tools/pushpull_tool.hpp"
#include "core/draw_ops.hpp"
#include "core/model_ops.hpp"
#include "core/numbers.hpp"
#include "core/select_ops.hpp"
#include "tools/handles.hpp"
#include "ui/theme.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

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
    if (state_ == State::Idle)
        return "drag a face (edge / point mode: an edge or point) along its normal  (2D: faces seen edge-on)  Cmd+drag: new faces";
    return "drag: distance  Return: apply  Esc: cancel  pushing something else applies this one first";
}

int PushPullTool::faceAt(const Mesh& m, const ViewGeom& g, double sx, double sy, const std::set<int>* prefer) {
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
        // Faces lying on the same line (a drawn shape and the face round it): the
        // selected one wins (a shape just drawn is selected), then the nearest.
        const bool sameDist = std::fabs(d - bestDist) <= 0.5;
        const bool mine = prefer && prefer->count(fi), theirs = best >= 0 && prefer && prefer->count(best);
        const bool better = d < bestDist - 0.5 || (sameDist && mine && !theirs) ||
                            (sameDist && mine == theirs && nearest > bestNear + 1e-6);
        if (better) {
            best = fi;
            bestDist = d;
            bestNear = nearest;
        }
    }
    return best;
}

PushPullTool::Target PushPullTool::targetAt(const Mesh& m, const ViewGeom& g, double sx, double sy, SelectMode mode,
                                            const std::set<int>* prefer) {
    Target t;
    Vec3 ro, rd;
    g.ray(sx, sy, ro, rd);
    if (mode == SelectMode::Edge) {
        for (const Edge& e : edgesAt(m, g, sx, sy)) {
            Target c;
            c.verts = {e.first, e.second};
            c.edge = true;
            Vec3 o, n;
            if (frame(m, c, o, n) && std::fabs(dot(n, rd)) < 0.9) return c; // can be dragged here
        }
        return t;
    }
    if (mode == SelectMode::Vert) {
        // Stacked points (2D): the one nearest the viewer.
        int best = -1;
        double bestNear = 0;
        for (int v : pointsAt(m, g, sx, sy)) {
            const double nearness = -dot(m.verts[v], rd);
            if (best < 0 || nearness > bestNear) best = v, bestNear = nearness;
        }
        if (best < 0) return t;
        Target c;
        c.verts = {best};
        Vec3 o, n;
        if (frame(m, c, o, n) && std::fabs(dot(n, rd)) < 0.9) return c;
        return t;
    }
    t.face = faceAt(m, g, sx, sy, prefer);
    return t;
}

bool PushPullTool::frame(const Mesh& m, const Target& t, Vec3& origin, Vec3& normal) {
    if (t.face >= 0) {
        if (t.face >= (int)m.faces.size()) return false;
        origin = faceCentre(m, t.face);
        normal = faceNormal(m, t.face);
    } else {
        if (t.verts.empty()) return false;
        Vec3 sum{0, 0, 0};
        for (int v : t.verts) sum = sum + m.verts[v];
        origin = sum * (1.0 / t.verts.size());
        normal = surfaceNormal(m, t.verts, t.edge);
    }
    return length(normal) > 0.5;
}

void PushPullTool::begin(ToolEnv& env, const Target& t, bool forceNew) {
    Document& doc = env.doc;
    target_ = t;
    moved_ = t.face;
    // A drawn shape is selected as several flat faces: pushing one of them pushes them
    // all (the selected faces lying flat with it and joined to it).
    region_ = {t.face};
    if (t.face >= 0 && doc.sel.mode == SelectMode::Face && doc.sel.faces.count(t.face)) {
        const Vec3 n = faceNormal(doc.mesh, t.face), c = faceCentre(doc.mesh, t.face);
        std::vector<int> todo = {t.face};
        while (!todo.empty()) {
            const int f = todo.back();
            todo.pop_back();
            for (int g : doc.sel.faces) {
                if (region_.count(g)) continue;
                if (dot(faceNormal(doc.mesh, g), n) < 1 - 1e-9 || std::fabs(dot(faceCentre(doc.mesh, g) - c, n)) > 1e-6) continue;
                // Joined: they share an edge.
                const Face& a = doc.mesh.faces[f];
                const Face& b = doc.mesh.faces[g];
                bool shares = false;
                for (int i = 0; i < a.n && !shares; ++i)
                    for (int j = 0; j < b.n; ++j)
                        if (a.v[i] == b.v[(j + 1) % b.n] && a.v[(i + 1) % a.n] == b.v[j]) shares = true;
                if (shares) {
                    region_.insert(g);
                    todo.push_back(g);
                }
            }
        }
    }
    movedRegion_ = region_;
    forceNew_ = forceNew;
    frame(doc.mesh, t, origin_, normal_);
    distance_ = 0;
    valid_ = true;
    kind_ = t.face >= 0 ? (int)pushKind(doc.mesh, region_, forceNew) : 0;
    // Pushed in this far, it reaches the other side: a hole (v0.2.3).
    throughAt_ = t.face >= 0 ? throughDistance(doc.mesh, region_) : 0;
    through_ = false;
    doc.beginLive();
}

void PushPullTool::refresh(ToolEnv& env) {
    Document& doc = env.doc;
    if (!doc.liveActive()) doc.beginLive();
    MeshEdit& e = doc.live();
    e.revert();
    valid_ = true;
    moved_ = target_.face;
    movedRegion_ = region_;
    through_ = false;
    // All the way to the other side (or past it): a hole; it stops there.
    if (target_.face >= 0 && throughAt_ > 0 && distance_ <= -throughAt_ + 1e-9) {
        distance_ = clean(-throughAt_);
        valid_ = pushThrough(e, region_);
        through_ = valid_;
        movedRegion_.clear();
        moved_ = -1;
        if (!valid_) e.revert();
        env.touch();
        return;
    }
    if (distance_ != 0) {
        if (target_.face >= 0) {
            std::set<int> moved;
            valid_ = pushPullFaces(e, region_, distance_, forceNew_, &moved);
            if (valid_) {
                movedRegion_ = moved;
                // The clicked face's place among them (they keep their order).
                const int pos = (int)std::distance(region_.begin(), region_.find(target_.face));
                moved_ = *std::next(moved.begin(), std::min(pos, (int)moved.size() - 1));
            }
        } else {
            valid_ = pushPoints(e, target_.verts, normal_, distance_);
        }
        if (!valid_) e.revert(); // shown as it was, outlined in red
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
    const SelectMode mode = doc.sel.mode;
    if (state_ == State::Pending) {
        const Target at = targetAt(doc.mesh, p.g, p.sx, p.sy, mode, &doc.sel.faces);
        if (!at.valid()) return;
        // What is being pushed: carry on with this push (edges and points keep their
        // indices; a face may have moved to a new one).
        const bool same = target_.face >= 0 ? (movedRegion_.count(at.face) || at.face == target_.face) : at.verts == target_.verts;
        if (same) {
            state_ = State::Dragging;
            view_ = p.view;
            startDistance_ = distance_;
            if (!lineParam(p.g, origin_, normal_, p.sx, p.sy, startT_)) state_ = State::Pending;
            return;
        }
        // Something else: the pending push goes in first.
        if (!apply(env)) cancel(env);
    }
    const Target t = targetAt(doc.mesh, p.g, p.sx, p.sy, mode, &doc.sel.faces);
    if (!t.valid()) return;
    Vec3 o, n;
    double tp;
    if (!frame(doc.mesh, t, o, n) || !lineParam(p.g, o, n, p.sx, p.sy, tp)) return; // pointing at the viewer
    begin(env, t, p.cmd);
    state_ = State::Dragging;
    view_ = p.view;
    startDistance_ = 0;
    startT_ = tp;
    hover_ = Target{};
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
    const Target at = targetAt(env.doc.mesh, p.g, p.sx, p.sy, env.doc.sel.mode, &env.doc.sel.faces);
    if (!(at == hover_) || p.view != hoverView_) {
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
    target_ = Target{};
    moved_ = -1;
    region_.clear();
    movedRegion_.clear();
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
    target_ = Target{};
    moved_ = -1;
    region_.clear();
    movedRegion_.clear();
    distance_ = 0;
    view_ = -1;
    env.touch();
}

void PushPullTool::overlay(const ToolEnv& env, const ViewGeom& g, int view, ImDrawList* dl) const {
    const Mesh& m = env.doc.mesh;
    auto mark = [&](const Target& t, ImU32 col, float w, int face) {
        if (face >= 0) {
            outlineFace(dl, g, m, face, col, w);
            return;
        }
        std::vector<ImVec2> pts;
        for (int v : t.verts) {
            double x, y;
            if (v < (int)m.verts.size() && g.project(m.verts[v], x, y)) pts.push_back(ImVec2((float)x, (float)y));
        }
        if (pts.size() == 2) dl->AddLine(pts[0], pts[1], col, w + 1.0f);
        for (const ImVec2& q : pts) dl->AddRectFilled({q.x - 4, q.y - 4}, {q.x + 4, q.y + 4}, col);
    };
    if (state_ != State::Idle) {
        const ImU32 col = valid_ ? hoverColour() : badColour();
        if (target_.face >= 0)
            for (int f : valid_ ? movedRegion_ : region_) mark(target_, col, 2.5f, f);
        else
            mark(target_, col, 2.5f, -1);
        drawDirArrow(dl, g, origin_ + normal_ * distance_, normal_, 40, col, state_ == State::Dragging);
    }
    if (hover_.valid() && view == hoverView_ && !(hover_ == target_) && (hover_.face < 0 || !movedRegion_.count(hover_.face)))
        mark(hover_, hoverColour(), 1.5f, hover_.face);
}

void PushPullTool::properties(ToolEnv& env) {
    ImGui::SeparatorText("Push/pull");
    if (state_ == State::Idle) {
        ImGui::TextWrapped("Drag a face to push or pull it. In a 2D view, drag a face you see edge-on (a line).");
        dimWrapped("Cmd+drag: always new side faces.");
        dimWrapped("In edge or point mode: drag an edge or a point along the surface's normal; the faces around tilt.");
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
    if (through_) {
        ImGui::TextWrapped("Through to the other side: a hole.");
    } else if (target_.face >= 0) {
        dimWrapped(what[std::clamp(kind_, 0, 2)]);
        if (throughAt_ > 0) {
            char buf[96];
            std::snprintf(buf, sizeof buf, "Pushed in %g cm it goes through: a hole.", throughAt_);
            dimWrapped(buf);
        }
    }
    else dimWrapped(target_.edge ? "The edge moves along the normal; the faces around tilt." : "The point moves along the normal; the faces around tilt.");
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
