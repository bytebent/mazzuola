#include "tools/draw_tool.hpp"
#include "core/draw_ops.hpp"
#include "core/numbers.hpp"
#include "core/select_ops.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace mazz {

namespace {
constexpr double kSnapPx = 8;         // how near a snap target must be on screen
constexpr double kAxisCos = 0.99863;  // cos 3°: a line piece this close to an axis clicks onto it
constexpr double kOnPlane = 2e-3;     // cm
constexpr double kDragPx = 4;         // press-drag-release counts as two clicks beyond this

const ImU32 kSnapCol[6] = {0, 0, IM_COL32(40, 170, 60, 255), IM_COL32(0, 170, 200, 255), IM_COL32(220, 50, 50, 255),
                           IM_COL32(50, 90, 230, 255)};
const ImU32 kAxisCol[3] = {IM_COL32(208, 62, 62, 255), IM_COL32(58, 156, 74, 255), IM_COL32(62, 102, 210, 255)};
const ImU32 kShapeCol = IM_COL32(236, 92, 178, 255);
const ImU32 kBadCol = IM_COL32(230, 60, 60, 255);

Vec3 unitOr(const Vec3& v) {
    const double l = length(v);
    return l > 0 ? v * (1.0 / l) : Vec3{0, 0, 0};
}

double comp(const Vec3& p, int a) { return a == 0 ? p.x : a == 1 ? p.y : p.z; }
void setComp(Vec3& p, int a, double v) { (a == 0 ? p.x : a == 1 ? p.y : p.z) = v; }

// The axis a normal points along (-1: none).
int normalAxis(const Vec3& n) {
    for (int a = 0; a < 3; ++a)
        if (std::fabs(std::fabs(comp(n, a)) - 1) < 1e-9) return a;
    return -1;
}

// The face under the cursor that can be drawn on: any face in 3D; in 2D one facing you.
int drawFaceAt(const Mesh& m, const ViewGeom& g, double sx, double sy) {
    Vec3 ro, rd;
    g.ray(sx, sy, ro, rd);
    for (int f : facesAt(m, g, sx, sy)) {
        if (!g.is3D() && std::fabs(dot(faceNormal(m, f), rd)) < 0.999) continue;
        return f;
    }
    return -1;
}

bool screen(const ViewGeom& g, const Vec3& p, ImVec2& out) {
    double x, y;
    if (!g.project(p, x, y)) return false;
    out = ImVec2((float)x, (float)y);
    return true;
}
} // namespace

const char* DrawTool::hint() const {
    switch (phase_) {
    case Phase::Idle:
        if (shape_ == Shape::Line) return "click on a face: start a line (from one of its edges)";
        return shape_ == Shape::Circle ? "click on a face: the circle's centre" : "click on a face: a corner of the rectangle";
    case Phase::Placing: return shape_ == Shape::Circle ? "click: the radius  Esc: cancel" : "click: the opposite corner  Esc: cancel";
    case Phase::Adjust: return "type the size or drag a corner  Return: cut it in  Esc: cancel";
    case Phase::Line: return "click: next point (end on an edge)  Shift: keep the direction  Return: finish  Backspace: undo a point  Esc: cancel";
    }
    return "";
}

DrawTool::Cursor DrawTool::snapAt(const ToolEnv& env, const ViewGeom& g, double sx, double sy, bool shift) const {
    const Mesh& m = env.doc.mesh;
    Cursor c;
    int face = face_;
    if (phase_ == Phase::Idle) face = drawFaceAt(m, g, sx, sy);
    Vec3 o, u, v, n;
    if (face < 0 || !faceFrame(m, face, o, u, v, n)) return c;
    Vec3 ro, rd;
    g.ray(sx, sy, ro, rd);
    const double den = dot(rd, n);
    if (std::fabs(den) < 1e-9) return c;
    Vec3 q = ro + rd * (dot(o - ro, n) / den);
    c.ok = true;
    c.p = q;

    // Snap targets near the cursor, best kind first, nearest on screen.
    double best = kSnapPx;
    Snap kind = Snap::None;
    auto consider = [&](const Vec3& p, Snap k) {
        double x, y;
        if (!g.project(p, x, y)) return;
        const double d = std::hypot(x - sx, y - sy);
        if (d > kSnapPx) return;
        const auto rank = [](Snap s) { return s == Snap::Point ? 0 : s == Snap::Midpoint ? 1 : s == Snap::Centre ? 2 : 3; };
        if (kind == Snap::None || rank(k) < rank(kind) || (rank(k) == rank(kind) && d < best)) {
            kind = k;
            best = d;
            c.p = p;
        }
    };
    for (const Vec3& p : m.verts)
        if (std::fabs(dot(p - o, n)) < kOnPlane) consider(p, Snap::Point);
    const Face& f = m.faces[face];
    for (int k = 0; k < f.n; ++k) {
        const Vec3 a = m.verts[f.v[k]], b = m.verts[f.v[(k + 1) % f.n]];
        consider((a + b) * 0.5, Snap::Midpoint);
        // Nearest point of the edge to the cursor's spot on the plane.
        const Vec3 d = b - a;
        const double t = std::clamp(dot(q - a, d) / dot(d, d), 0.0, 1.0);
        Vec3 on = a + d * t;
        // On a grid line along the edge when snapping (an edge along an axis).
        if (env.grid.snap) {
            for (int ax = 0; ax < 3; ++ax)
                if (std::fabs(std::fabs(dot(unitOr(d), axisVec(ax))) - 1) < 1e-9) {
                    const double s = env.grid.snapValue(comp(on, ax));
                    const double lo = std::min(comp(a, ax), comp(b, ax)), hi = std::max(comp(a, ax), comp(b, ax));
                    if (s >= lo - 1e-9 && s <= hi + 1e-9) setComp(on, ax, s);
                }
        }
        consider(on, Snap::Edge);
    }
    consider(faceCentre(m, face), Snap::Centre);
    c.snap = kind;

    // Direction: a line piece near X / Y / Z clicks onto it (Shift keeps it).
    const bool hasPrev = phase_ == Phase::Line && !line_.empty();
    if (hasPrev) {
        const Vec3 prev = line_.back();
        int axis = shift && lockAxis_ >= 0 ? lockAxis_ : -1;
        const Vec3 dir = c.p - prev;
        if (axis < 0 && length(dir) > 1e-6 && (kind == Snap::None)) {
            // Near the axis line through the last point, on screen (or within a few degrees).
            double bestPx = kSnapPx;
            double px0, py0;
            const bool prevOn = g.project(prev, px0, py0);
            for (int a = 0; a < 3; ++a) {
                Vec3 ap = axisVec(a) - n * dot(axisVec(a), n);
                if (length(ap) < 0.1) continue;
                ap = ap * (1.0 / length(ap));
                const double along = dot(dir, ap);
                double px1, py1;
                if (prevOn && g.project(prev + ap * (along == 0 ? 1 : along), px1, py1)) {
                    const double lx = px1 - px0, ly = py1 - py0, l = std::hypot(lx, ly);
                    if (l > 1) {
                        const double off = std::fabs((sx - px0) * ly - (sy - py0) * lx) / l;
                        if (off < bestPx) bestPx = off, axis = a;
                        continue;
                    }
                }
                if (std::fabs(along) / length(dir) >= kAxisCos) axis = a;
            }
        }
        if (axis >= 0) {
            Vec3 ap = axisVec(axis) - n * dot(axisVec(axis), n);
            if (length(ap) > 0.1) {
                ap = ap * (1.0 / length(ap));
                c.p = prev + ap * dot(c.p - prev, ap);
                c.axis = axis;
                // Along a world axis lying in the face: whole grid steps along it.
                if (kind == Snap::None && env.grid.snap && std::fabs(std::fabs(dot(ap, axisVec(axis))) - 1) < 1e-9)
                    setComp(c.p, axis, env.grid.snapValue(comp(c.p, axis)));
                return c;
            }
        }
    }
    if (kind != Snap::None) return c;
    // Grid: world grid on faces along an axis, else steps in the face's own frame.
    if (env.grid.snap) {
        const int na = normalAxis(n);
        if (na >= 0) {
            for (int a = 0; a < 3; ++a)
                if (a != na) setComp(c.p, a, env.grid.snapValue(comp(c.p, a)));
        } else {
            const double x = env.grid.snapValue(dot(c.p - o, u)), y = env.grid.snapValue(dot(c.p - o, v));
            c.p = o + u * x + v * y;
        }
        c.snap = Snap::Grid;
    }
    return c;
}

std::vector<Vec3> DrawTool::points() const {
    switch (shape_) {
    case Shape::Rectangle: return rectanglePoints(a_, u_, v_, w_, h_);
    case Shape::Circle: return circlePoints(a_, u_, v_, r_, segments_);
    case Shape::Line: return line_;
    }
    return {};
}

void DrawTool::check(ToolEnv& env) {
    fits_ = true;
    why_.clear();
    if (phase_ == Phase::Idle || phase_ == Phase::Line) return;
    if ((shape_ == Shape::Rectangle && (std::fabs(w_) < 1e-6 || std::fabs(h_) < 1e-6)) ||
        (shape_ == Shape::Circle && r_ < 1e-6))
        return;
    // A trial cut on a copy: the same answer Return would give.
    Mesh copy = env.doc.mesh;
    MeshEdit e(copy);
    fits_ = cutShape(e, face_, points(), true, nullptr, &why_);
}

void DrawTool::reset(ToolEnv& env) {
    phase_ = Phase::Idle;
    face_ = -1;
    line_.clear();
    w_ = h_ = r_ = 0;
    fits_ = true;
    why_.clear();
    dragCorner_ = -1;
    view_ = -1;
    env.touch();
}

void DrawTool::cancel(ToolEnv& env) {
    if (phase_ != Phase::Idle) reset(env);
}

bool DrawTool::apply(ToolEnv& env) {
    if (phase_ == Phase::Idle || phase_ == Phase::Placing) return false;
    const bool closed = shape_ != Shape::Line;
    const std::vector<Vec3> pts = points();
    if (!closed && pts.size() < 2) return false;
    if (closed) check(env);
    if (!fits_) return false;
    std::string why;
    const char* name = shape_ == Shape::Rectangle ? "Draw rectangle" : shape_ == Shape::Circle ? "Draw circle" : "Draw line";
    const int face = face_;
    const bool ok = env.doc.apply(name, [&](MeshEdit& e, Selection& sel) {
        std::set<int> shape;
        if (!cutShape(e, face, pts, closed, &shape, &why)) return false;
        sel = Selection();
        sel.mode = SelectMode::Face;
        sel.faces = shape;
        return true;
    });
    if (!ok) {
        fits_ = false;
        why_ = why;
        env.touch();
        return false;
    }
    reset(env);
    return true;
}

void DrawTool::press(ToolEnv& env, const Pointer& p) {
    const Cursor c = snapAt(env, p.g, p.sx, p.sy, p.shift);
    pressX_ = p.sx;
    pressY_ = p.sy;
    moved_ = false;
    switch (phase_) {
    case Phase::Adjust: {
        // A corner of the rectangle: drag it (the opposite one stays).
        if (shape_ == Shape::Rectangle) {
            const std::vector<Vec3> pts = points();
            for (int k = 0; k < 4; ++k) {
                double x, y;
                if (p.g.project(pts[k], x, y) && std::hypot(x - p.sx, y - p.sy) <= kSnapPx) {
                    a_ = pts[(k + 2) % 4];
                    dragCorner_ = k;
                    view_ = p.view;
                    return;
                }
            }
        }
        // Elsewhere: this one goes in first (if it fits), then a new one may start.
        if (!apply(env)) return;
        [[fallthrough]];
    }
    case Phase::Idle: {
        const Cursor s = phase_ == Phase::Idle ? snapAt(env, p.g, p.sx, p.sy, p.shift) : c;
        const int face = drawFaceAt(env.doc.mesh, p.g, p.sx, p.sy);
        if (face < 0 || !s.ok) return;
        if (!faceFrame(env.doc.mesh, face, o_, u_, v_, n_)) return;
        face_ = face;
        view_ = p.view;
        if (shape_ == Shape::Line) {
            line_ = {s.p};
            phase_ = Phase::Line;
        } else {
            a_ = s.p;
            w_ = h_ = r_ = 0;
            phase_ = Phase::Placing;
        }
        cursor_ = s;
        env.touch();
        return;
    }
    case Phase::Placing:
        if (!c.ok) return;
        if (shape_ == Shape::Rectangle) {
            w_ = clean(dot(c.p - a_, u_));
            h_ = clean(dot(c.p - a_, v_));
            if (std::fabs(w_) < 1e-6 || std::fabs(h_) < 1e-6) return;
        } else {
            r_ = clean(length(c.p - a_));
            if (r_ < 1e-6) return;
        }
        phase_ = Phase::Adjust;
        check(env);
        env.touch();
        return;
    case Phase::Line:
        if (!c.ok) return;
        if (p.clicks >= 2) {
            apply(env);
            return;
        }
        if (length(c.p - line_.back()) > 1e-3) line_.push_back(c.p);
        fits_ = true;
        why_.clear();
        env.touch();
        return;
    }
}

void DrawTool::drag(ToolEnv& env, const Pointer& p) {
    if (std::hypot(p.sx - pressX_, p.sy - pressY_) > kDragPx) moved_ = true;
    if (phase_ == Phase::Adjust && dragCorner_ >= 0 && p.view == view_) {
        const Cursor c = snapAt(env, p.g, p.sx, p.sy, p.shift);
        if (!c.ok) return;
        w_ = clean(dot(c.p - a_, u_));
        h_ = clean(dot(c.p - a_, v_));
        cursor_ = c;
        check(env);
        env.touch();
        return;
    }
    hover(env, p);
}

void DrawTool::release(ToolEnv& env, const Pointer& p) {
    if (phase_ == Phase::Adjust && dragCorner_ >= 0) {
        dragCorner_ = -1;
        env.touch();
        return;
    }
    // Press, drag, release: the release is the second click.
    if (phase_ == Phase::Placing && moved_ && p.view == view_) press(env, p);
}

void DrawTool::hover(ToolEnv& env, const Pointer& p) {
    // Shift keeps the direction found so far.
    if (!p.shift) lockAxis_ = -1;
    else if (lockAxis_ < 0 && cursor_.axis >= 0) lockAxis_ = cursor_.axis;
    if (phase_ != Phase::Idle && p.view != view_) return;
    Cursor c = snapAt(env, p.g, p.sx, p.sy, p.shift);
    c.view = p.view;
    const bool changed = c.ok != cursor_.ok || c.snap != cursor_.snap || c.axis != cursor_.axis || c.view != cursor_.view ||
                         (c.ok && length(c.p - cursor_.p) > 1e-9);
    cursor_ = c;
    if (phase_ == Phase::Placing && c.ok) {
        if (shape_ == Shape::Rectangle) {
            w_ = clean(dot(c.p - a_, u_));
            h_ = clean(dot(c.p - a_, v_));
        } else {
            r_ = clean(length(c.p - a_));
        }
        if (changed) check(env);
    }
    if (changed) env.touch();
}

bool DrawTool::key(ToolEnv& env, int key, bool shift, bool cmd) {
    if (cmd || phase_ == Phase::Idle) return false;
    if (key == GLFW_KEY_ESCAPE && !shift) {
        reset(env);
        return true;
    }
    if (key == GLFW_KEY_ENTER && !shift) {
        apply(env);
        return true;
    }
    if (key == GLFW_KEY_BACKSPACE && phase_ == Phase::Line) {
        line_.pop_back();
        if (line_.empty()) reset(env);
        env.touch();
        return true;
    }
    return false;
}

void DrawTool::overlay(const ToolEnv& env, const ViewGeom& g, int view, ImDrawList* dl) const {
    (void)env;
    const ImU32 col = fits_ ? kShapeCol : kBadCol;
    if (phase_ != Phase::Idle) {
        std::vector<Vec3> pts = points();
        const bool closed = shape_ != Shape::Line;
        std::vector<ImVec2> s;
        for (const Vec3& p : pts) {
            ImVec2 q;
            if (screen(g, p, q)) s.push_back(q);
        }
        if (s.size() == pts.size() && s.size() >= 2) dl->AddPolyline(s.data(), (int)s.size(), col, closed ? ImDrawFlags_Closed : 0, 2.0f);
        for (const ImVec2& q : s) dl->AddRectFilled({q.x - 2.5f, q.y - 2.5f}, {q.x + 2.5f, q.y + 2.5f}, col);
        // The piece to the cursor while drawing a line, in the axis colour when on one.
        if (phase_ == Phase::Line && cursor_.ok && view == cursor_.view && !s.empty()) {
            ImVec2 q;
            if (screen(g, cursor_.p, q)) dl->AddLine(s.back(), q, cursor_.axis >= 0 ? kAxisCol[cursor_.axis] : col, 2.0f);
        }
        if (phase_ == Phase::Adjust && shape_ == Shape::Rectangle)
            for (const ImVec2& q : s) {
                dl->AddRectFilled({q.x - 4, q.y - 4}, {q.x + 4, q.y + 4}, IM_COL32(255, 255, 255, 255));
                dl->AddRect({q.x - 4, q.y - 4}, {q.x + 4, q.y + 4}, col, 0, 0, 1.5f);
            }
    }
    // The snap marker: only when a snap caught something (not the plain grid).
    if (cursor_.ok && view == cursor_.view && cursor_.snap != Snap::None && cursor_.snap != Snap::Grid &&
        phase_ != Phase::Adjust) {
        ImVec2 q;
        if (screen(g, cursor_.p, q)) {
            const ImU32 c = kSnapCol[(int)cursor_.snap];
            if (cursor_.snap == Snap::Edge) dl->AddRectFilled({q.x - 4, q.y - 4}, {q.x + 4, q.y + 4}, c);
            else dl->AddCircleFilled(q, 5, c);
            dl->AddCircle(q, 5.5f, IM_COL32(255, 255, 255, 255), 0, 1.0f);
        }
    }
}

void DrawTool::setShape(ToolEnv& env, Shape s) {
    if (s == shape_) return;
    reset(env);
    shape_ = s;
}

void DrawTool::setSize(ToolEnv& env, double w, double h) {
    if (shape_ != Shape::Rectangle || phase_ != Phase::Adjust) return;
    w_ = clean(w);
    h_ = clean(h);
    check(env);
    env.touch();
}

void DrawTool::setRadius(ToolEnv& env, double r) {
    if (shape_ != Shape::Circle || phase_ != Phase::Adjust) return;
    r_ = clean(std::fabs(r));
    check(env);
    env.touch();
}

void DrawTool::setSegments(ToolEnv& env, int n) {
    segments_ = std::clamp(n, 3, 64);
    if (phase_ == Phase::Adjust) check(env);
    env.touch();
}

void DrawTool::properties(ToolEnv& env) {
    ImGui::SeparatorText("Draw");
    int s = (int)shape_;
    bool sh = false;
    sh |= ImGui::RadioButton("Rectangle", &s, 0);
    ImGui::SameLine();
    sh |= ImGui::RadioButton("Circle", &s, 1);
    ImGui::SameLine();
    sh |= ImGui::RadioButton("Line", &s, 2);
    if (sh) setShape(env, (Shape)s);
    const float fieldW = ImGui::GetFontSize() * 7;
    auto onReturn = [&]() {
        if (ImGui::IsItemDeactivatedAfterEdit() &&
            (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
            apply(env);
    };
    if (shape_ == Shape::Circle) {
        int n = segments_;
        ImGui::SetNextItemWidth(fieldW);
        if (ImGui::InputInt("Segments", &n)) setSegments(env, n);
    }
    if (phase_ == Phase::Adjust && shape_ == Shape::Rectangle) {
        double w = std::fabs(w_), h = std::fabs(h_);
        ImGui::SetNextItemWidth(fieldW);
        if (ImGui::InputDouble("Width (cm)", &w, 0, 0, "%g") && w > 0) setSize(env, w_ < 0 ? -w : w, h_);
        onReturn();
        ImGui::SetNextItemWidth(fieldW);
        if (ImGui::InputDouble("Height (cm)", &h, 0, 0, "%g") && h > 0) setSize(env, w_, h_ < 0 ? -h : h);
        onReturn();
    }
    if (phase_ == Phase::Adjust && shape_ == Shape::Circle) {
        double r = r_;
        ImGui::SetNextItemWidth(fieldW);
        if (ImGui::InputDouble("Radius (cm)", &r, 0, 0, "%g") && r > 0) setRadius(env, r);
        onReturn();
    }
    if (phase_ == Phase::Idle) {
        dimWrapped(shape_ == Shape::Line ? "Click on a face to start a line; it must go from one edge of the face to another."
                                         : "Click on a face to start. It fits inside that face; touching its edge is fine.");
        dimWrapped("Snaps: green a point, cyan a midpoint, red an edge, blue the centre.");
        return;
    }
    if (!fits_) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.2f, 0.2f, 1));
        ImGui::TextWrapped("Doesn't fit: %s.", why_.empty() ? "it doesn't fit the face" : why_.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::BeginDisabled(phase_ == Phase::Placing);
    if (ImGui::Button("Cut it in (Return)")) apply(env);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel (Esc)")) reset(env);
}

} // namespace mazz
