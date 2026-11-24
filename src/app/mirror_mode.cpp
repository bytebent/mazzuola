// Mirror mode in the app (v0.2.7, ARCHITECTURE §6 Mirror mode): the toggle and plane,
// the side being worked on (the selected point nearest the mouse at a press), the
// fix-up after each tool event, and the plane drawn in the views.
#include "app/editor.hpp"
#include "core/mirror_ops.hpp"
#include "core/select_ops.hpp"
#include "imgui.h"
#include <cmath>

namespace mazz {

void Editor::setMirror(bool on, int plane) {
    cancelTool(); // a preview in progress was made without (or with) it
    mirrorOn_ = on;
    mirrorPlane_ = std::clamp(plane, 0, 2);
    doc_.mirrorAxis = on ? mirrorPlane_ : -1;
    if (on) doc_.select(doc_.sel); // the twins join the selection
    unpairedVersion_ = -1;
    markViewsDirty();
    touch();
}

void Editor::mirrorSideAt(int view, double sx, double sy) {
    if (!mirrorOn_) return;
    const ViewGeom g = geom(view);
    const Mesh& m = doc_.mesh;
    std::set<int> cand = selectedVerts(m, doc_.sel);
    if (cand.empty())
        for (int i = 0; i < (int)m.verts.size(); ++i) cand.insert(i);
    double best = 1e300;
    int side = 0;
    for (int v : cand) {
        const double c = m.verts[v][mirrorPlane_];
        if (std::abs(c) <= kOnPlane) continue;
        double px, py;
        if (!g.project(m.verts[v], px, py)) continue;
        const double d = (px - sx) * (px - sx) + (py - sy) * (py - sy);
        if (d < best) best = d, side = c > 0 ? 1 : -1;
    }
    if (side != 0) doc_.mirrorSide = side;
}

void Editor::mirrorAfterTool() {
    if (!mirrorOn_ || !doc_.liveActive()) return;
    const long before = doc_.meshVersion();
    doc_.mirrorFixLive();
    if (doc_.meshVersion() != before) touch();
}

int Editor::unpairedCount() {
    if (!mirrorOn_) return 0;
    if (unpairedVersion_ != doc_.meshVersion()) {
        unpaired_ = unpairedPoints(doc_.mesh, mirrorPlane_);
        unpairedVersion_ = doc_.meshVersion();
    }
    return unpaired_;
}

void Editor::drawMirrorPlane(int view, ImDrawList* dl) {
    if (!mirrorOn_) return;
    const ViewGeom g = geom(view);
    const int a = mirrorPlane_;
    const ImU32 col[3] = {IM_COL32(220, 60, 60, 210), IM_COL32(40, 160, 60, 210), IM_COL32(60, 90, 220, 210)};
    const ImU32 fill[3] = {IM_COL32(220, 60, 60, 28), IM_COL32(40, 160, 60, 28), IM_COL32(60, 90, 220, 28)};
    // The plane's extent: the model's bounds, a little larger (an empty model: 64 cm).
    Vec3 mn{-64, -64, 0}, mx{64, 64, 64};
    if (!doc_.mesh.verts.empty()) doc_.mesh.bounds(mn, mx);
    const double pad = 8;
    const int b = (a + 1) % 3, c = (a + 2) % 3;
    auto corner = [&](double u, double v) {
        Vec3 p{0, 0, 0};
        p[b] = u;
        p[c] = v;
        return p;
    };
    const double b0 = mn[b] - pad, b1 = mx[b] + pad, c0 = mn[c] - pad, c1 = mx[c] + pad;
    if (g.is3D()) {
        const Vec3 q[4] = {corner(b0, c0), corner(b1, c0), corner(b1, c1), corner(b0, c1)};
        ImVec2 s[4];
        for (int k = 0; k < 4; ++k) {
            double x, y;
            if (!g.project(q[k], x, y)) return;
            s[k] = ImVec2((float)x, (float)y);
        }
        dl->AddQuadFilled(s[0], s[1], s[2], s[3], fill[a]);
        dl->AddQuad(s[0], s[1], s[2], s[3], col[a], 1.5f);
        return;
    }
    // A 2D view: the plane is a line unless it faces the view (its axis is the view's
    // depth: then nothing to draw).
    double x0, y0, x1, y1;
    {
        Vec3 u{0, 0, 0};
        u[a] = 100;
        if (!g.project(Vec3{0, 0, 0}, x0, y0) || !g.project(u, x1, y1)) return;
        if (std::abs(x1 - x0) + std::abs(y1 - y0) < 1e-6) return;
    }
    const double far = 1e5;
    Vec3 p0{0, 0, 0}, p1{0, 0, 0};
    // Along whichever of the other two axes the view shows.
    for (int other : {b, c}) {
        p0 = Vec3{0, 0, 0}, p1 = Vec3{0, 0, 0};
        p0[other] = -far;
        p1[other] = far;
        if (!g.project(p0, x0, y0) || !g.project(p1, x1, y1)) return;
        if (std::abs(x1 - x0) + std::abs(y1 - y0) > 1) break;
    }
    if (std::abs(x1 - x0) + std::abs(y1 - y0) <= 1) return; // facing the view
    // Dashed, so it doesn't read as the axis line under it.
    const double len = std::hypot(x1 - x0, y1 - y0), dx = (x1 - x0) / len, dy = (y1 - y0) / len;
    const ImVec2 lo = dl->GetClipRectMin(), hi = dl->GetClipRectMax();
    // Only the part on screen: from the point nearest the view's middle, both ways.
    const double mxs = (lo.x + hi.x) / 2, mys = (lo.y + hi.y) / 2;
    const double t0 = (mxs - x0) * dx + (mys - y0) * dy;
    const double reach = std::hypot(hi.x - lo.x, hi.y - lo.y);
    const float dash = 9 * ImGui::GetIO().FontGlobalScale, gap = 5 * ImGui::GetIO().FontGlobalScale;
    for (double t = t0 - reach; t < t0 + reach; t += dash + gap) {
        const ImVec2 a0((float)(x0 + dx * t), (float)(y0 + dy * t));
        const ImVec2 a1((float)(x0 + dx * (t + dash)), (float)(y0 + dy * (t + dash)));
        dl->AddLine(a0, a1, col[a], 2.0f);
    }
}

} // namespace mazz
