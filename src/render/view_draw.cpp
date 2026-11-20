#include "render/view_draw.hpp"
#include "core/figure.hpp"
#include "core/grid_lines.hpp"
#include "render/gl.hpp"
#include <algorithm>
#include <cmath>

namespace mazz {

void View::setDir(ViewDir d) {
    if (d == dir) return;
    if (!is3D() && d != ViewDir::Persp) {
        // Keep looking at the same spot: move the centre through world space.
        const Vec3 c = ortho.planeToWorld(ortho.cu, ortho.cv);
        ortho.dir = d;
        ortho.worldToPlane(c, ortho.cu, ortho.cv);
    } else if (d != ViewDir::Persp) {
        ortho.dir = d;
    }
    dir = d;
    dirty = true;
}

ViewDrawer::~ViewDrawer() {
    delete renderer_;
    delete model_;
    delete images_;
}

ModelRenderer& ViewDrawer::model() {
    if (!model_) model_ = new ModelRenderer();
    return *model_;
}

void ViewDrawer::setModel(const DrawData& d) { model().setGeometry(d); }

void ViewDrawer::setSelection(const std::vector<unsigned>& tris, const std::vector<unsigned>& edges) {
    model().setSelection(tris, edges);
}

namespace {
Rgba withAlpha(Rgba c, double a) {
    c.a = (uint8_t)std::clamp(a * 255.0, 0.0, 255.0);
    return c;
}
} // namespace

void ViewDrawer::build2D(const View& v, const Grid& grid, double w, double h) {
    const OrthoCamera& cam = v.ortho;
    const OrthoAxes ax = orthoAxes(cam.dir);
    double u0, v0, u1, v1;
    cam.screenToPlane(0, h, w, h, u0, v0);
    cam.screenToPlane(w, 0, w, h, u1, v1);
    // Lines sit slightly behind the plane through the origin, so model geometry (later)
    // at any depth draws on top in 2D (no depth test in 2D views).
    for (const GridLine2D& l : gridLines2D(grid, cam, w, h)) {
        Rgba c = l.kind == GridLineKind::Major ? viewcol::kMajor : viewcol::kMinor;
        if (l.kind == GridLineKind::AxisU) c = viewcol::kAxis[ax.u];
        if (l.kind == GridLineKind::AxisV) c = viewcol::kAxis[ax.v];
        if (l.vertical) batch_.line(cam.planeToWorld(l.value, v0), cam.planeToWorld(l.value, v1), c);
        else batch_.line(cam.planeToWorld(u0, l.value), cam.planeToWorld(u1, l.value), c);
    }
}

void ViewDrawer::build3D(const View& v, const Grid& grid) {
    const Camera3D& cam = v.cam3d;
    double step = 0, extent = 0;
    const std::vector<GridLine3D> lines = gridLines3D(grid, cam, step, extent);
    const Vec3 centre{std::round(cam.target.x / step) * step, std::round(cam.target.y / step) * step, 0};
    // Fade with distance from the centre so the grid has no hard edge.
    constexpr int kPieces = 24;
    for (const GridLine3D& l : lines) {
        Rgba c = l.kind == GridLineKind::Major ? viewcol::kMajor : viewcol::kMinor;
        if (l.kind == GridLineKind::AxisU) c = viewcol::kAxis[0];
        if (l.kind == GridLineKind::AxisV) c = viewcol::kAxis[1];
        for (int i = 0; i < kPieces; ++i) {
            const Vec3 a = l.a + (l.b - l.a) * ((double)i / kPieces);
            const Vec3 b = l.a + (l.b - l.a) * ((double)(i + 1) / kPieces);
            auto fade = [&](const Vec3& p) {
                const double d = length(p - centre) / extent;
                return std::pow(std::clamp(1.0 - d, 0.0, 1.0), 1.5);
            };
            batch_.line(a, b, withAlpha(c, fade(a)), withAlpha(c, fade(b)));
        }
    }
    // Z axis: a short blue line up from the origin.
    const double zLen = step * 8;
    batch_.line({0, 0, 0}, {0, 0, zLen}, viewcol::kAxis[2], withAlpha(viewcol::kAxis[2], 0.2));
}

void ViewDrawer::buildFigure(const ViewExtras& x, bool fill) {
    figure_.clear();
    if (!x.figure) return;
    for (const auto& b : figureBoxes(x.figureAt)) {
        const Vec3& a = b.first;
        const Vec3& c = b.second;
        Vec3 p[8];
        for (int i = 0; i < 8; ++i) p[i] = {(i & 1) ? c.x : a.x, (i & 2) ? c.y : a.y, (i & 4) ? c.z : a.z};
        static const int kEdges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                                          {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (const auto& e : kEdges) figure_.line(p[e[0]], p[e[1]], viewcol::kFigure);
        if (!fill) continue;
        static const int kQuads[6][4] = {{0, 2, 3, 1}, {4, 5, 7, 6}, {0, 1, 5, 4},
                                         {2, 6, 7, 3}, {0, 4, 6, 2}, {1, 3, 7, 5}};
        for (const auto& q : kQuads) {
            figure_.tri(p[q[0]], p[q[1]], p[q[2]], viewcol::kFigureFill);
            figure_.tri(p[q[0]], p[q[2]], p[q[3]], viewcol::kFigureFill);
        }
    }
}

void ViewDrawer::drawRef(const View& v, const ViewExtras& x, const Mat4& viewProj) {
    if (!x.refTex || v.is3D()) return;
    if (!images_) images_ = new ImageRenderer();
    double u[4], vv[4];
    x.ref.corners(u, vv);
    Vec3 c[4];
    for (int k = 0; k < 4; ++k) c[k] = v.ortho.planeToWorld(u[k], vv[k]);
    images_->draw(x.refTex, c, (float)x.ref.alpha, viewProj);
}

void ViewDrawer::draw(View& v, const Grid& grid, double w, double h, double pxScale, const ViewExtras& extras) {
    const int pw = std::max(1, (int)std::lround(w * pxScale));
    const int ph = std::max(1, (int)std::lround(h * pxScale));
    const bool resized = v.target.resize(pw, ph);
    if (!v.dirty && !resized) return;
    v.dirty = false;
    if (!renderer_) renderer_ = new BatchRenderer();

    batch_.clear();
    Mat4 viewProj;
    const Rgba bg = v.is3D() ? viewcol::kBg3D : viewcol::kBg2D;
    if (v.is3D()) {
        build3D(v, grid);
        viewProj = v.cam3d.projection(w / std::max(1.0, h)) * v.cam3d.view();
    } else {
        build2D(v, grid, w, h);
        viewProj = v.ortho.projection(w, h) * v.ortho.view();
    }
    buildFigure(extras, v.is3D());
    v.target.begin(bg.r / 255.f, bg.g / 255.f, bg.b / 255.f);
    renderer_->draw(batch_, viewProj, v.is3D());
    if (v.is3D()) {
        // A light over the viewer's shoulder, so faces facing the camera are brightest.
        const Camera3D& c = v.cam3d;
        model().draw3D(viewProj, c.forward() * -1.0 + c.up() * 0.6 + c.right() * 0.3);
        // The figure after the model: see-through, hidden where the model is in front.
        renderer_->draw(figure_, viewProj, true, false);
    } else {
        // Reference image behind or in front of the model; the figure on top (outlines).
        if (!extras.ref.front) drawRef(v, extras, viewProj);
        model().draw2D(viewProj);
        if (extras.ref.front) drawRef(v, extras, viewProj);
        renderer_->draw(figure_, viewProj, false);
    }
    v.target.end();
}

} // namespace mazz
