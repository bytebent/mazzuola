#include "tools/vertex_tool.hpp"
#include "core/select_ops.hpp"
#include <algorithm>
#include <cmath>

namespace mazz {

const char* VertexTool::hint() const {
    if (slide_ >= 0) return "sliding the point along an edge (the one closest to your drag)";
    return "drag a point: move it  Alt+drag: slide it along an edge  click: select  Cmd+click: add/remove  "
           "drag on empty space: rectangle  arrows: along an axis  Esc: select nothing";
}

void VertexTool::press(ToolEnv& env, const Pointer& p) {
    // Alt on a point: slide it along one of its edges.
    if (p.alt && !p.cmd) {
        const std::set<int> pts = pointsAt(env.doc.mesh, p.g, p.sx, p.sy);
        if (!pts.empty()) {
            Vec3 ro, rd;
            p.g.ray(p.sx, p.sy, ro, rd);
            int best = -1;
            double bestNear = 0;
            for (int v : pts) {
                const double nearness = -dot(env.doc.mesh.verts[v], rd);
                if (best < 0 || nearness > bestNear) best = v, bestNear = nearness;
            }
            Selection s;
            s.mode = SelectMode::Vert;
            s.verts = {best};
            env.doc.select(s);
            slide_ = best;
            slideTo_ = -1;
            slideView_ = p.view;
            slideFrom_ = env.doc.mesh.verts[best];
            slidePressX_ = p.sx;
            slidePressY_ = p.sy;
            env.doc.beginLive();
            env.touch();
            return;
        }
    }
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

void VertexTool::drag(ToolEnv& env, const Pointer& p) {
    if (slide_ < 0) {
        SelectTool::drag(env, p);
        return;
    }
    if (p.view != slideView_) return;
    const Mesh& m = env.doc.mesh;
    double ox, oy;
    if (!p.g.project(slideFrom_, ox, oy)) return;
    const double dx = p.sx - slidePressX_, dy = p.sy - slidePressY_;
    if (slideTo_ < 0) {
        if (std::hypot(dx, dy) < 4) return;
        // The edge whose direction on screen is closest to the drag.
        std::set<int> others;
        for (const Face& f : m.faces)
            for (int k = 0; k < f.n; ++k)
                if (f.v[k] == slide_) others.insert(f.v[(k + 1) % f.n]), others.insert(f.v[(k + f.n - 1) % f.n]);
        double bestCos = 0.3;
        for (int o : others) {
            double x, y;
            if (!p.g.project(m.verts[o], x, y)) continue;
            const double ex = x - ox, ey = y - oy, l = std::hypot(ex, ey);
            if (l < 2) continue;
            const double c = (ex * dx + ey * dy) / (l * std::hypot(dx, dy));
            if (c > bestCos) bestCos = c, slideTo_ = o;
        }
        if (slideTo_ < 0) return;
    }
    double tx, ty;
    if (!p.g.project(m.verts[slideTo_], tx, ty)) return;
    const double ex = tx - ox, ey = ty - oy, l2 = ex * ex + ey * ey;
    if (l2 < 1) return;
    double t = std::clamp((dx * ex + dy * ey) / l2, 0.0, 1.0);
    // Grid steps along the edge when snapping.
    const Vec3 to = m.verts[slideTo_];
    const double len = length(to - slideFrom_);
    if (env.grid.snap && len > 0) t = std::clamp(env.grid.snapValue(t * len) / len, 0.0, 1.0);
    if (t >= 1.0) t = 1.0 - 1e-3 / std::max(len, 1e-3); // never onto the other point
    MeshEdit& e = env.doc.live();
    e.setVert(slide_, slideFrom_ + (to - slideFrom_) * t);
    env.touch();
}

void VertexTool::release(ToolEnv& env, const Pointer& p) {
    if (slide_ < 0) {
        SelectTool::release(env, p);
        return;
    }
    if (slideTo_ >= 0 && length(env.doc.mesh.verts[slide_] - slideFrom_) > 1e-9) env.doc.commitLive("Slide point");
    else env.doc.cancelLive();
    slide_ = slideTo_ = slideView_ = -1;
    env.touch();
}

void VertexTool::cancel(ToolEnv& env) {
    if (slide_ >= 0) {
        env.doc.cancelLive();
        slide_ = slideTo_ = slideView_ = -1;
    }
    SelectTool::cancel(env);
}

void VertexTool::overlay(const ToolEnv& env, const ViewGeom& g, int view, ImDrawList* dl) const {
    if (slide_ < 0) {
        SelectTool::overlay(env, g, view, dl);
        return;
    }
    // The edge it slides along.
    if (slideTo_ < 0) return;
    double ax, ay, bx, by;
    if (g.project(slideFrom_, ax, ay) && g.project(env.doc.mesh.verts[slideTo_], bx, by))
        dl->AddLine(ImVec2((float)ax, (float)ay), ImVec2((float)bx, (float)by), IM_COL32(0, 170, 200, 255), 3.0f);
}

} // namespace mazz
