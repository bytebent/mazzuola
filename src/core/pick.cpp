#include "core/pick.hpp"
#include <algorithm>
#include <cmath>

namespace mazz {

bool Projector::project(const Vec3& p, double& x, double& y, double& depth) const {
    double wclip = 1;
    const Vec3 ndc = transformPoint(viewProj, p, &wclip);
    if (perspective && wclip <= 1e-9) return false;
    x = (ndc.x * 0.5 + 0.5) * w;
    y = (0.5 - ndc.y * 0.5) * h;
    depth = perspective ? wclip : ndc.z;
    return true;
}

void Projector::ray(double x, double y, Vec3& origin, Vec3& dir) const {
    Mat4 inv;
    invert(viewProj, inv);
    const double nx = x / w * 2 - 1, ny = 1 - y / h * 2;
    const Vec3 a = transformPoint(inv, {nx, ny, -1});
    const Vec3 b = transformPoint(inv, {nx, ny, 1});
    origin = a;
    dir = normalize(b - a);
}

namespace {
// Ray against triangle; t along the ray or -1.
double rayTri(const Vec3& o, const Vec3& d, const Vec3& a, const Vec3& b, const Vec3& c) {
    const Vec3 e1 = b - a, e2 = c - a;
    const Vec3 h = cross(d, e2);
    const double det = dot(e1, h);
    if (std::fabs(det) < 1e-14) return -1;
    const double inv = 1.0 / det;
    const Vec3 s = o - a;
    const double u = inv * dot(s, h);
    if (u < -1e-9 || u > 1 + 1e-9) return -1;
    const Vec3 q = cross(s, e1);
    const double v = inv * dot(d, q);
    if (v < -1e-9 || u + v > 1 + 1e-9) return -1;
    const double t = inv * dot(e2, q);
    return t > 0 ? t : -1;
}

FaceHit rayMesh(const Mesh& m, const Vec3& o, const Vec3& d, int skipVertex = -1, const Edge* skipEdge = nullptr) {
    FaceHit best;
    for (int f = 0; f < (int)m.faces.size(); ++f) {
        const Face& fc = m.faces[f];
        if (fc.hidden) continue;
        bool skip = false;
        for (int k = 0; k < fc.n; ++k) {
            if (fc.v[k] == skipVertex) skip = true;
            if (skipEdge && (fc.v[k] == skipEdge->first || fc.v[k] == skipEdge->second)) skip = true;
        }
        if (skip) continue;
        std::array<std::array<int, 3>, 2> tris;
        const int nt = fc.triangles(tris);
        for (int t = 0; t < nt; ++t) {
            const double hit = rayTri(o, d, m.verts[fc.v[tris[t][0]]], m.verts[fc.v[tris[t][1]]], m.verts[fc.v[tris[t][2]]]);
            if (hit > 0 && (best.face < 0 || hit < best.t)) { best.face = f; best.t = hit; }
        }
    }
    return best;
}

std::vector<bool> visibleVerts(const Mesh& m) {
    std::vector<bool> vis(m.verts.size(), false);
    for (const Face& f : m.faces)
        if (!f.hidden)
            for (int k = 0; k < f.n; ++k) vis[f.v[k]] = true;
    return vis;
}

// Is the point hidden behind a face (3D views)? Faces using `skipVertex`/`skipEdge` don't count.
bool occluded(const Mesh& m, const Projector& pr, const Vec3& p, int skipVertex, const Edge* skipEdge) {
    if (!pr.perspective) return false;
    double x, y, dep;
    if (!pr.project(p, x, y, dep)) return true;
    Vec3 o, d;
    pr.ray(x, y, o, d);
    const double dist = dot(p - o, d);
    const FaceHit h = rayMesh(m, o, d, skipVertex, skipEdge);
    return h.face >= 0 && h.t < dist * (1 - 1e-6) - 1e-6;
}

double segDist(double px, double py, double ax, double ay, double bx, double by, double& t) {
    const double dx = bx - ax, dy = by - ay;
    const double len2 = dx * dx + dy * dy;
    t = len2 > 0 ? std::clamp(((px - ax) * dx + (py - ay) * dy) / len2, 0.0, 1.0) : 0.0;
    const double cx = ax + dx * t - px, cy = ay + dy * t - py;
    return std::sqrt(cx * cx + cy * cy);
}
} // namespace

FaceHit pickFace(const Mesh& m, const Projector& pr, double x, double y) {
    Vec3 o, d;
    pr.ray(x, y, o, d);
    return rayMesh(m, o, d);
}

int pickVertex(const Mesh& m, const Projector& pr, double x, double y, double radius) {
    const std::vector<bool> vis = visibleVerts(m);
    int best = -1;
    double bestD = radius, bestDepth = 1e300;
    for (int v = 0; v < (int)m.verts.size(); ++v) {
        if (!vis[v]) continue;
        double sx, sy, dep;
        if (!pr.project(m.verts[v], sx, sy, dep)) continue;
        const double d = std::hypot(sx - x, sy - y);
        if (d > radius) continue;
        const bool better = d < bestD - 1e-9 || (std::fabs(d - bestD) <= 1e-9 && dep < bestDepth);
        if (!better) continue;
        if (occluded(m, pr, m.verts[v], v, nullptr)) continue;
        best = v;
        bestD = d;
        bestDepth = dep;
    }
    return best;
}

bool pickEdge(const Mesh& m, const Projector& pr, double x, double y, double radius, Edge& out) {
    bool found = false;
    double bestD = radius;
    for (const Edge& e : m.edges(false)) {
        double ax, ay, ad, bx, by, bd;
        if (!pr.project(m.verts[e.first], ax, ay, ad) || !pr.project(m.verts[e.second], bx, by, bd)) continue;
        double t;
        const double d = segDist(x, y, ax, ay, bx, by, t);
        if (d > bestD) continue;
        const Vec3 p = m.verts[e.first] + (m.verts[e.second] - m.verts[e.first]) * t;
        if (occluded(m, pr, p, -1, &e)) continue;
        bestD = d;
        out = e;
        found = true;
    }
    return found;
}

Selection boxSelect(const Mesh& m, const Projector& pr, double x0, double y0, double x1, double y1, SelectMode mode) {
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    const std::vector<bool> vis = visibleVerts(m);
    std::vector<bool> inside(m.verts.size(), false);
    for (int v = 0; v < (int)m.verts.size(); ++v) {
        double sx, sy, dep;
        if (vis[v] && pr.project(m.verts[v], sx, sy, dep)) inside[v] = sx >= x0 && sx <= x1 && sy >= y0 && sy <= y1;
    }
    Selection s;
    switch (mode) {
    case SelectMode::Vertex:
        for (int v = 0; v < (int)m.verts.size(); ++v)
            if (inside[v]) s.verts.insert(v);
        break;
    case SelectMode::Edge:
        for (const Edge& e : m.edges(false))
            if (inside[e.first] && inside[e.second]) s.edges.insert(e);
        break;
    case SelectMode::Face:
    case SelectMode::Piece:
        for (int f = 0; f < (int)m.faces.size(); ++f) {
            const Face& fc = m.faces[f];
            if (fc.hidden) continue;
            bool all = true;
            for (int k = 0; k < fc.n; ++k) all = all && inside[fc.v[k]];
            if (all) s.faces.insert(f);
        }
        break;
    }
    return s;
}

} // namespace mazz
