#include "render/view_draw.hpp"
#include "core/grid_lines.hpp"
#include "render/gl.hpp"
#include "render/mesh_draw.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

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

Mat4 View::viewProj(double w, double h) const {
    if (is3D()) return cam3d.projection(w / std::max(1.0, h)) * cam3d.view();
    return ortho.projection(w, h) * ortho.view();
}

std::string resolveRefPath(const std::string& path, const std::string& baseDir) {
    if (path.empty() || path[0] == '/' || baseDir.empty()) return path;
    return baseDir + "/" + path;
}

ViewDrawer::ViewDrawer() = default;

ViewDrawer::~ViewDrawer() {
    delete lines_;
    delete lit_;
    delete images_;
}

namespace {
Rgba withAlpha(Rgba c, double a) {
    c.a = (uint8_t)std::clamp(a * c.a, 0.0, 255.0);
    return c;
}

// A simple 175 cm figure: a front silhouette (in the XZ plane) and a side one (YZ), so it
// reads in every view. Feet at `b`.
void addFigure(Batch& batch, const Vec3& b, Rgba c) {
    struct Seg {
        double x0, z0, x1, z1;
    };
    // Front: legs, hips, torso, shoulders, arms, neck. (cm; x across, z up)
    const Seg front[] = {
        {-13, 0, -9, 88},   {13, 0, 9, 88},    {-9, 88, 9, 88},     {-9, 88, -21, 145},
        {9, 88, 21, 145},   {-21, 145, 21, 145}, {-21, 145, -25, 82}, {21, 145, 25, 82},
        {0, 145, 0, 153},   {-20, 0, -8, 0},   {8, 0, 20, 0},
    };
    for (const Seg& s : front) batch.line(b + Vec3{s.x0, 0, s.z0}, b + Vec3{s.x1, 0, s.z1}, c);
    // Side: a slimmer outline (y across).
    const Seg side[] = {
        {-6, 0, -6, 88}, {8, 0, 7, 88}, {-6, 88, -8, 145}, {7, 88, 9, 145}, {-8, 145, 9, 145}, {-6, 0, 16, 0},
    };
    for (const Seg& s : side) batch.line(b + Vec3{0, s.x0, s.z0}, b + Vec3{0, s.x1, s.z1}, c);
    // Head: a circle in each plane, top at 175.
    const int n = 16;
    for (int i = 0; i < n; ++i) {
        const double a0 = 2 * kPi * i / n, a1 = 2 * kPi * (i + 1) / n;
        const double r = 11, cz = 164;
        batch.line(b + Vec3{r * std::cos(a0), 0, cz + r * std::sin(a0)}, b + Vec3{r * std::cos(a1), 0, cz + r * std::sin(a1)}, c);
        batch.line(b + Vec3{0, r * std::cos(a0), cz + r * std::sin(a0)}, b + Vec3{0, r * std::cos(a1), cz + r * std::sin(a1)}, c);
    }
}

void addMeshLines(Batch& batch, const Mesh& m, Rgba normal, const Selection* sel, SelectMode mode) {
    std::set<Edge> pink;
    if (sel) {
        if (mode == SelectMode::Edge) pink = sel->edges;
        if (mode == SelectMode::Face || mode == SelectMode::Piece)
            for (int f : sel->faces)
                if (f >= 0 && f < (int)m.faces.size())
                    for (int k = 0; k < m.faces[f].n; ++k)
                        pink.insert(makeEdge(m.faces[f].v[k], m.faces[f].v[(k + 1) % m.faces[f].n]));
    }
    for (const Edge& e : m.edges(false)) {
        const bool s = pink.count(e) > 0;
        batch.line(m.verts[e.first], m.verts[e.second], s ? viewcol::kSelection : normal);
    }
}

void addLitFaces(std::vector<LitVertex>& out, const Mesh& m, Rgba base, const std::set<int>* selected, Rgba tint) {
    const auto normals = cornerNormals(m);
    for (int f = 0; f < (int)m.faces.size(); ++f) {
        const Face& fc = m.faces[f];
        if (fc.hidden) continue;
        Rgba c = base;
        if (selected && selected->count(f)) {
            c.r = (uint8_t)((c.r + tint.r) / 2);
            c.g = (uint8_t)((c.g + tint.g) / 2);
            c.b = (uint8_t)((c.b + tint.b) / 2);
        }
        std::array<std::array<int, 3>, 2> tris;
        const int nt = fc.triangles(tris);
        for (int t = 0; t < nt; ++t)
            for (int k = 0; k < 3; ++k) {
                const int corner = tris[t][k];
                const Vec3& p = m.verts[fc.v[corner]];
                const Vec3& n = normals[f][corner];
                out.push_back({(float)p.x, (float)p.y, (float)p.z, (float)n.x, (float)n.y, (float)n.z, c.r, c.g, c.b, c.a});
            }
    }
}
} // namespace

void ViewDrawer::build2D(const View& v, const Grid& grid, const Scene& scene, double w, double h) {
    const OrthoCamera& cam = v.ortho;
    const OrthoAxes ax = orthoAxes(cam.dir);
    double u0, v0, u1, v1;
    cam.screenToPlane(0, h, w, h, u0, v0);
    cam.screenToPlane(w, 0, w, h, u1, v1);
    for (const GridLine2D& l : gridLines2D(grid, cam, w, h)) {
        Rgba c = l.kind == GridLineKind::Major ? viewcol::kMajor : viewcol::kMinor;
        if (l.kind == GridLineKind::AxisU) c = viewcol::kAxis[ax.u];
        if (l.kind == GridLineKind::AxisV) c = viewcol::kAxis[ax.v];
        if (l.vertical) batch_.line(cam.planeToWorld(l.value, v0), cam.planeToWorld(l.value, v1), c);
        else batch_.line(cam.planeToWorld(u0, l.value), cam.planeToWorld(u1, l.value), c);
    }
}

void ViewDrawer::build3D(const View& v, const Grid& grid, const Scene& scene) {
    const Camera3D& cam = v.cam3d;
    double step = 0, extent = 0;
    const std::vector<GridLine3D> lines = gridLines3D(grid, cam, step, extent);
    const Vec3 centre{std::round(cam.target.x / step) * step, std::round(cam.target.y / step) * step, 0};
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
    batch_.line({0, 0, 0}, {0, 0, step * 8}, viewcol::kAxis[2], withAlpha(viewcol::kAxis[2], 0.2));
    (void)scene;
}

void ViewDrawer::drawRefs(const View& v, const Scene& scene, bool front, const Mat4& viewProj) {
    if (v.is3D() || !scene.model || !scene.textures || !scene.showRefs) return;
    for (const RefImage& r : scene.model->refs) {
        if (r.dir != v.dir || r.inFront != front || r.alpha <= 0) continue;
        const TextureCache::Entry& e = scene.textures->get(resolveRefPath(r.path, scene.refBaseDir));
        if (e.failed || !e.tex || e.w <= 0) continue;
        const double hw = r.width / 2, hh = r.width * e.h / e.w / 2;
        const double c = std::cos(radians(r.rotation)), s = std::sin(radians(r.rotation));
        const double cu[4] = {-hw, hw, hw, -hw}, cv[4] = {-hh, -hh, hh, hh};
        Vec3 corners[4];
        for (int i = 0; i < 4; ++i)
            corners[i] = v.ortho.planeToWorld(r.x + cu[i] * c - cv[i] * s, r.y + cu[i] * s + cv[i] * c);
        images_->draw(e.tex, corners, (float)std::clamp(r.alpha, 0.0, 1.0), viewProj);
    }
}

void ViewDrawer::draw(View& v, const Grid& grid, const Scene& scene, double w, double h, double pxScale) {
    const int pw = std::max(1, (int)std::lround(w * pxScale));
    const int ph = std::max(1, (int)std::lround(h * pxScale));
    const bool resized = v.target.resize(pw, ph);
    if (!v.dirty && !resized && v.drawnVersion == scene.version) return;
    v.dirty = false;
    v.drawnVersion = scene.version;
    if (!lines_) lines_ = new BatchRenderer();
    if (!lit_) lit_ = new LitRenderer();
    if (!images_) images_ = new ImageRenderer();

    const Rgba bg = v.is3D() ? viewcol::kBg3D : viewcol::kBg2D;
    const Mat4 viewProj = v.viewProj(w, h);
    const Mesh* mesh = scene.model ? &scene.model->mesh : nullptr;
    v.target.begin(bg.r / 255.f, bg.g / 255.f, bg.b / 255.f);

    if (v.is3D()) {
        // Faces first (pushed back a little), then grid and lines with depth test.
        std::vector<LitVertex> tris;
        if (mesh) {
            std::set<int> selFaces;
            if (scene.sel && (scene.mode == SelectMode::Face || scene.mode == SelectMode::Piece)) selFaces = scene.sel->faces;
            addLitFaces(tris, *mesh, viewcol::kFace, &selFaces, viewcol::kSelection);
        }
        if (scene.preview) addLitFaces(tris, *scene.preview, Rgba{170, 205, 222, 255}, nullptr, viewcol::kPreview);
        lit_->draw(tris, viewProj, -v.cam3d.forward());
        batch_.clear();
        build3D(v, grid, scene);
        if (mesh) {
            const Rgba wire = scene.wireOverlay ? viewcol::kWire3D : Rgba{0, 0, 0, 0};
            // Selected edges always show, even without the wire overlay.
            addMeshLines(batch_, *mesh, wire, scene.sel, scene.mode);
        }
        if (scene.preview) addMeshLines(batch_, *scene.preview, viewcol::kPreview, nullptr, scene.mode);
        if (scene.showFigure) addFigure(batch_, scene.figureAt, viewcol::kFigure);
        lines_->draw(batch_, viewProj, true);
    } else {
        batch_.clear();
        build2D(v, grid, scene, w, h);
        lines_->draw(batch_, viewProj, false);
        drawRefs(v, scene, false, viewProj);
        batch_.clear();
        if (mesh && scene.sel && (scene.mode == SelectMode::Face || scene.mode == SelectMode::Piece)) {
            for (int f : scene.sel->faces) {
                if (f < 0 || f >= (int)mesh->faces.size() || mesh->faces[f].hidden) continue;
                const Face& fc = mesh->faces[f];
                std::array<std::array<int, 3>, 2> t;
                const int nt = fc.triangles(t);
                for (int k = 0; k < nt; ++k)
                    batch_.tri(mesh->verts[fc.v[t[k][0]]], mesh->verts[fc.v[t[k][1]]], mesh->verts[fc.v[t[k][2]]],
                               viewcol::kSelectionFill);
            }
        }
        if (mesh) addMeshLines(batch_, *mesh, viewcol::kEdge2D, scene.sel, scene.mode);
        if (scene.preview) addMeshLines(batch_, *scene.preview, viewcol::kPreview, nullptr, scene.mode);
        if (scene.showFigure) addFigure(batch_, scene.figureAt, viewcol::kFigure);
        lines_->draw(batch_, viewProj, false);
        drawRefs(v, scene, true, viewProj);
    }
    v.target.end();
}

} // namespace mazz
