#include "core/rock.hpp"
#include "core/ops.hpp"
#include "core/rng.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace mazz {

// ---------------------------------------------------------------- convex hull

Mesh convexHull(const std::vector<Vec3>& in) {
    Mesh m;
    if (in.size() < 4) return m;
    // Scale for tolerances.
    double ext = 0;
    for (const Vec3& p : in) ext = std::max(ext, length(p));
    const double eps = std::max(1e-12, ext * 1e-9);

    std::vector<Vec3> pts = in;
    // Starting tetrahedron: extreme points, as far apart as possible.
    int i0 = 0, i1 = -1, i2 = -1, i3 = -1;
    for (int i = 1; i < (int)pts.size(); ++i)
        if (pts[i].x < pts[i0].x) i0 = i;
    double best = -1;
    for (int i = 0; i < (int)pts.size(); ++i) {
        const double d = length(pts[i] - pts[i0]);
        if (d > best) { best = d; i1 = i; }
    }
    best = -1;
    for (int i = 0; i < (int)pts.size(); ++i) {
        const double d = length(cross(pts[i1] - pts[i0], pts[i] - pts[i0]));
        if (d > best) { best = d; i2 = i; }
    }
    best = -1;
    const Vec3 n012 = cross(pts[i1] - pts[i0], pts[i2] - pts[i0]);
    for (int i = 0; i < (int)pts.size(); ++i) {
        const double d = std::fabs(dot(n012, pts[i] - pts[i0]));
        if (d > best) { best = d; i3 = i; }
    }
    if (best <= eps * length(n012)) return m; // all points flat

    struct Tri {
        int a, b, c;
        Vec3 n;
        double d;
        bool alive;
    };
    std::vector<Tri> tris;
    auto addTri = [&](int a, int b, int c) {
        Tri t{a, b, c, normalize(cross(pts[b] - pts[a], pts[c] - pts[a])), 0, true};
        t.d = dot(t.n, pts[a]);
        tris.push_back(t);
    };
    // Orient the tetrahedron outward.
    if (dot(n012, pts[i3] - pts[i0]) > 0) std::swap(i1, i2);
    addTri(i0, i1, i2);
    addTri(i0, i3, i1);
    addTri(i1, i3, i2);
    addTri(i2, i3, i0);

    std::vector<bool> done(pts.size(), false);
    done[i0] = done[i1] = done[i2] = done[i3] = true;
    for (int p = 0; p < (int)pts.size(); ++p) {
        if (done[p]) continue;
        done[p] = true;
        std::vector<int> visible;
        for (int t = 0; t < (int)tris.size(); ++t)
            if (tris[t].alive && dot(tris[t].n, pts[p]) - tris[t].d > eps) visible.push_back(t);
        if (visible.empty()) continue; // inside
        // Horizon: directed edges of visible triangles whose reverse isn't visible.
        std::set<std::pair<int, int>> edges;
        for (int t : visible) {
            const int v[3] = {tris[t].a, tris[t].b, tris[t].c};
            for (int k = 0; k < 3; ++k) edges.insert({v[k], v[(k + 1) % 3]});
            tris[t].alive = false;
        }
        for (const auto& e : edges)
            if (!edges.count({e.second, e.first})) addTri(e.first, e.second, p);
    }
    // Compact into a mesh.
    std::map<int, int> remap;
    for (const Tri& t : tris) {
        if (!t.alive) continue;
        const int v[3] = {t.a, t.b, t.c};
        int id[3];
        for (int k = 0; k < 3; ++k) {
            auto it = remap.find(v[k]);
            if (it == remap.end()) it = remap.emplace(v[k], m.addVertex(pts[v[k]])).first;
            id[k] = it->second;
        }
        m.addTri(id[0], id[1], id[2]);
    }
    return m;
}

// ---------------------------------------------------------------- self-intersection

namespace {
// Segment p0-p1 against triangle a, b, c (strict: touching at the ends doesn't count).
bool segmentHitsTriangle(const Vec3& p0, const Vec3& p1, const Vec3& a, const Vec3& b, const Vec3& c) {
    const Vec3 dir = p1 - p0;
    const Vec3 e1 = b - a, e2 = c - a;
    const Vec3 h = cross(dir, e2);
    const double det = dot(e1, h);
    const double scale = length(e1) * length(e2) * length(dir);
    if (std::fabs(det) < 1e-12 * std::max(1e-30, scale)) return false; // parallel
    const double inv = 1.0 / det;
    const Vec3 s = p0 - a;
    const double u = inv * dot(s, h);
    if (u <= 1e-9 || u >= 1 - 1e-9) return false;
    const Vec3 q = cross(s, e1);
    const double v = inv * dot(dir, q);
    if (v <= 1e-9 || u + v >= 1 - 1e-9) return false;
    const double t = inv * dot(e2, q);
    return t > 1e-9 && t < 1 - 1e-9;
}
} // namespace

bool selfIntersects(const Mesh& m) {
    struct T {
        int v[3];
    };
    std::vector<T> tris;
    for (const Face& f : m.faces) {
        std::array<std::array<int, 3>, 2> t;
        const int nt = f.triangles(t);
        for (int k = 0; k < nt; ++k) tris.push_back({{f.v[t[k][0]], f.v[t[k][1]], f.v[t[k][2]]}});
    }
    for (size_t i = 0; i < tris.size(); ++i)
        for (size_t j = 0; j < tris.size(); ++j) {
            if (i == j) continue;
            bool shares = false;
            for (int a = 0; a < 3; ++a)
                for (int b = 0; b < 3; ++b)
                    if (tris[i].v[a] == tris[j].v[b]) shares = true;
            if (shares) continue;
            const Vec3 A = m.verts[tris[j].v[0]], B = m.verts[tris[j].v[1]], C = m.verts[tris[j].v[2]];
            for (int k = 0; k < 3; ++k)
                if (segmentHitsTriangle(m.verts[tris[i].v[k]], m.verts[tris[i].v[(k + 1) % 3]], A, B, C)) return true;
        }
    return false;
}

// ---------------------------------------------------------------- rock

namespace {
// Cuts a closed convex triangle mesh with the plane z = h, keeping the part above, and
// closes it with a flat floor (a triangle fan around the floor's centre).
Mesh cutBottom(const Mesh& in, double h) {
    // Edge -> new point at the cut.
    std::vector<Vec3> pts;
    std::map<int, int> keep;      // old vertex -> new
    std::map<Edge, int> cutPoint; // edge -> new
    auto above = [&](int v) { return in.verts[v].z > h; };
    auto keepId = [&](int v) {
        auto it = keep.find(v);
        if (it != keep.end()) return it->second;
        pts.push_back(in.verts[v]);
        return keep[v] = (int)pts.size() - 1;
    };
    auto cutId = [&](int a, int b) {
        const Edge e = makeEdge(a, b);
        auto it = cutPoint.find(e);
        if (it != cutPoint.end()) return it->second;
        const Vec3 pa = in.verts[e.first], pb = in.verts[e.second];
        const double t = (h - pa.z) / (pb.z - pa.z);
        pts.push_back(pa + (pb - pa) * t);
        pts.back().z = h;
        return cutPoint[e] = (int)pts.size() - 1;
    };
    std::vector<std::vector<int>> polys;
    for (const Face& f : in.faces) {
        std::vector<int> poly;
        for (int k = 0; k < f.n; ++k) {
            const int a = f.v[k], b = f.v[(k + 1) % f.n];
            if (above(a)) poly.push_back(keepId(a));
            if (above(a) != above(b)) poly.push_back(cutId(a, b));
        }
        if (poly.size() >= 3) polys.push_back(poly);
    }
    Mesh m;
    m.verts = pts;
    for (const auto& poly : polys)
        for (size_t k = 1; k + 1 < poly.size(); ++k) m.addTri(poly[0], poly[k], poly[k + 1]);
    // Floor: the cut points, sorted around their centre, facing down.
    std::vector<int> ring;
    Vec3 c;
    for (const auto& [e, id] : cutPoint) {
        ring.push_back(id);
        c += m.verts[id];
    }
    if (ring.size() >= 3) {
        c = c / (double)ring.size();
        std::sort(ring.begin(), ring.end(), [&](int a, int b) {
            return std::atan2(m.verts[a].y - c.y, m.verts[a].x - c.x) < std::atan2(m.verts[b].y - c.y, m.verts[b].x - c.x);
        });
        // Fan from the first point (the floor is convex): K - 2 triangles, facing down.
        for (size_t k = 1; k + 1 < ring.size(); ++k) m.addTri(ring[0], ring[k + 1], ring[k]);
    }
    return m;
}
} // namespace

namespace {
// The rock's base shape (hull of jittered sphere points, optionally cut flat) from `n`
// points. The same seed always makes the same points.
Mesh rockBase(const PrimParams& p, int n, uint32_t variant) {
    Rng rng((uint64_t)p.seed * 0x9E3779B97F4A7C15ULL + variant);
    const double golden = kPi * (3.0 - std::sqrt(5.0));
    std::vector<Vec3> pts;
    for (int i = 0; i < n; ++i) {
        const double z = 1.0 - 2.0 * (i + 0.5) / n + rng.signedUnit() * 0.6 / n;
        const double r = std::sqrt(std::max(0.0, 1.0 - z * z));
        const double a = golden * i + rng.signedUnit() * golden * 0.35;
        Vec3 q{r * std::cos(a), r * std::sin(a), z};
        q = normalize(q) * (1.0 + rng.signedUnit() * 0.08);
        pts.push_back({q.x * p.stretch.x / 100.0, q.y * p.stretch.y / 100.0, q.z * p.stretch.z / 100.0});
    }
    Mesh m = convexHull(pts);
    if (p.flatBottom && p.bottomHeight > 0) {
        double zmin = 1e30, zmax = -1e30;
        for (const Vec3& v : m.verts) { zmin = std::min(zmin, v.z); zmax = std::max(zmax, v.z); }
        m = cutBottom(m, zmin + (zmax - zmin) * p.bottomHeight / 100.0);
    }
    return m;
}
} // namespace

Mesh makeRock(const PrimParams& in) {
    const PrimParams p = clampParams(in);
    // A hull of N points on a sphere has 2N - 4 triangles; a flat bottom changes that, so
    // try a few point counts (and point layouts) and keep the one closest to the target.
    // Deterministic: the same seed always walks the same candidates.
    const int guess = std::max(6, (int)std::lround(p.faces / 2.0 + 2));
    Mesh m;
    int bestDiff = 1 << 30;
    for (uint32_t variant = 0; variant < 6 && bestDiff > 0; ++variant) {
        for (int n = guess; n >= std::max(4, guess - 12); --n) {
            Mesh cand = rockBase(p, n, variant);
            const int diff = std::abs(cand.triangleCount() - p.faces);
            if (diff < bestDiff) { bestDiff = diff; m = std::move(cand); }
            if (diff == 0 || !p.flatBottom) break;
        }
        if (!p.flatBottom || bestDiff * 10 <= p.faces) break; // within 10 %: good
    }
    // Roughen: move points along their normals by up to a fraction of their shortest edge,
    // so the surface can't fold over itself. The floor stays flat.
    if (p.roughness > 0) {
        std::vector<Vec3> nrm(m.verts.size());
        std::vector<double> shortest(m.verts.size(), 1e30);
        for (int f = 0; f < (int)m.faces.size(); ++f) {
            const Face& fc = m.faces[f];
            const Vec3 fn = m.faceNormal(f) * m.faceArea(f);
            for (int k = 0; k < fc.n; ++k) {
                nrm[fc.v[k]] += fn;
                const double len = length(m.verts[fc.v[(k + 1) % fc.n]] - m.verts[fc.v[k]]);
                shortest[fc.v[k]] = std::min(shortest[fc.v[k]], len);
                shortest[fc.v[(k + 1) % fc.n]] = std::min(shortest[fc.v[(k + 1) % fc.n]], len);
            }
        }
        double zmin = 1e30;
        for (const Vec3& v : m.verts) zmin = std::min(zmin, v.z);
        const Mesh before = m;
        for (int attempt = 0; attempt < 4; ++attempt) {
            const double amount = p.roughness / 100.0 * 0.22 / (1 << attempt);
            Rng r2(p.seed * 2654435761u + 17);
            m = before;
            for (size_t v = 0; v < m.verts.size(); ++v) {
                if (p.flatBottom && std::fabs(m.verts[v].z - zmin) < 1e-9) continue;
                m.verts[v] += normalize(nrm[v]) * (r2.signedUnit() * amount * shortest[v]);
            }
            if (!selfIntersects(m)) break;
            if (attempt == 3) m = before;
        }
    }
    if (p.joinQuads) joinTrianglesIntoQuads(m, 8.0);
    for (Face& f : m.faces) f.sg = p.smooth ? 1 : 0;
    // Fit exactly into the unit box.
    Vec3 mn, mx;
    if (m.bounds(mn, mx)) {
        const Vec3 c = (mn + mx) / 2, s = mx - mn;
        for (Vec3& v : m.verts) {
            v -= c;
            for (int a = 0; a < 3; ++a) v[a] = s[a] > 1e-12 ? v[a] / s[a] : 0;
        }
    }
    return m;
}

} // namespace mazz
