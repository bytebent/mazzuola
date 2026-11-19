#include "core/rock.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <random>
#include <set>

namespace mazz {

RockParams clampRock(const RockParams& in) {
    RockParams p = in;
    p.faces = std::clamp(p.faces, 12, 60);
    p.roughness = std::clamp(p.roughness, 0.0, 100.0);
    p.bottomHeight = std::clamp(p.bottomHeight, 0.0, 60.0);
    return p;
}

Mesh convexHull(const std::vector<Vec3>& pts) {
    Mesh out;
    const int n = (int)pts.size();
    if (n < 4) return out;
    const double eps = 1e-9;
    // A first tetrahedron from four points that aren't flat.
    int a = 0, b = -1, c = -1, d = -1;
    for (int i = 1; i < n && b < 0; ++i)
        if (length(pts[i] - pts[a]) > eps) b = i;
    for (int i = 0; i < n && b >= 0 && c < 0; ++i)
        if (length(cross(pts[b] - pts[a], pts[i] - pts[a])) > eps) c = i;
    for (int i = 0; i < n && c >= 0 && d < 0; ++i)
        if (std::fabs(dot(cross(pts[b] - pts[a], pts[c] - pts[a]), pts[i] - pts[a])) > eps) d = i;
    if (d < 0) return out;
    std::vector<std::array<int, 3>> faces;
    auto normal = [&](const std::array<int, 3>& f) { return cross(pts[f[1]] - pts[f[0]], pts[f[2]] - pts[f[0]]); };
    const Vec3 inside = (pts[a] + pts[b] + pts[c] + pts[d]) / 4;
    for (auto f : {std::array<int, 3>{a, b, c}, {a, b, d}, {a, c, d}, {b, c, d}}) {
        if (dot(normal(f), pts[f[0]] - inside) < 0) std::swap(f[1], f[2]);
        faces.push_back(f);
    }
    for (int p = 0; p < n; ++p) {
        if (p == a || p == b || p == c || p == d) continue;
        std::vector<char> visible(faces.size(), 0);
        bool any = false;
        for (size_t i = 0; i < faces.size(); ++i) {
            const Vec3 nrm = normal(faces[i]);
            if (dot(nrm, pts[p] - pts[faces[i][0]]) > eps * length(nrm)) {
                visible[i] = 1;
                any = true;
            }
        }
        if (!any) continue; // inside, or on the surface
        // Horizon: edges of visible faces whose other side isn't visible.
        std::set<std::pair<int, int>> visEdges;
        for (size_t i = 0; i < faces.size(); ++i)
            if (visible[i])
                for (int k = 0; k < 3; ++k) visEdges.insert({faces[i][k], faces[i][(k + 1) % 3]});
        std::vector<std::array<int, 3>> next;
        for (size_t i = 0; i < faces.size(); ++i)
            if (!visible[i]) next.push_back(faces[i]);
        for (const auto& e : visEdges)
            if (!visEdges.count({e.second, e.first})) next.push_back({e.first, e.second, p});
        faces = next;
    }
    // Only the points the hull uses.
    std::map<int, int> remap;
    for (const auto& f : faces)
        for (int k = 0; k < 3; ++k)
            if (!remap.count(f[k])) {
                remap[f[k]] = (int)out.verts.size();
                out.verts.push_back(pts[f[k]]);
            }
    for (const auto& f : faces) {
        Face fc;
        fc.n = 3;
        for (int k = 0; k < 3; ++k) fc.v[k] = remap[f[k]];
        out.faces.push_back(fc);
    }
    return out;
}

namespace {
// Faces that would turn over (or collapse) if points moved to `pos`.
bool anyFolded(const Mesh& m, const std::vector<Vec3>& pos) {
    for (const Face& f : m.faces) {
        const Vec3 before = cross(m.verts[f.v[1]] - m.verts[f.v[0]], m.verts[f.v[2]] - m.verts[f.v[0]]);
        const Vec3 after = cross(pos[f.v[1]] - pos[f.v[0]], pos[f.v[2]] - pos[f.v[0]]);
        if (dot(normalize(before), normalize(after)) < 0.35 || length(after) < 1e-6) return true;
    }
    return false;
}

// Joins neighbouring triangles into quads where they lie nearly flat together and the
// quad is convex.
void joinIntoQuads(Mesh& m) {
    std::map<std::pair<int, int>, int> owner; // directed edge -> face
    for (int i = 0; i < (int)m.faces.size(); ++i)
        for (int k = 0; k < 3; ++k) owner[{m.faces[i].v[k], m.faces[i].v[(k + 1) % 3]}] = i;
    std::vector<char> used(m.faces.size(), 0);
    std::vector<Face> out;
    for (int i = 0; i < (int)m.faces.size(); ++i) {
        if (used[i]) continue;
        const Face& f = m.faces[i];
        int best = -1, bestK = -1;
        double bestDot = std::cos(radians(20));
        for (int k = 0; k < 3; ++k) {
            auto it = owner.find({f.v[(k + 1) % 3], f.v[k]});
            if (it == owner.end() || used[it->second] || it->second == i) continue;
            const double d = dot(faceNormal(m, i), faceNormal(m, it->second));
            if (d > bestDot) {
                bestDot = d;
                best = it->second;
                bestK = k;
            }
        }
        if (best < 0) {
            out.push_back(f);
            used[i] = 1;
            continue;
        }
        // Triangle (a b c) shares edge a-b (k, k+1) with the other, whose third point is x.
        const int a = f.v[bestK], b = f.v[(bestK + 1) % 3], c = f.v[(bestK + 2) % 3];
        const Face& g = m.faces[best];
        int x = -1;
        for (int k = 0; k < 3; ++k)
            if (g.v[k] != a && g.v[k] != b) x = g.v[k];
        Face q;
        q.n = 4;
        q.v[0] = a;
        q.v[1] = x;
        q.v[2] = b;
        q.v[3] = c;
        q.sg = f.sg;
        // Convex: every corner turns the same way as the face.
        const Vec3 n = faceNormal(m, i) + faceNormal(m, best);
        bool convex = true;
        for (int k = 0; k < 4; ++k) {
            const Vec3& p0 = m.verts[q.v[k]];
            const Vec3& p1 = m.verts[q.v[(k + 1) % 4]];
            const Vec3& p2 = m.verts[q.v[(k + 2) % 4]];
            if (dot(cross(p1 - p0, p2 - p1), n) <= 0) convex = false;
        }
        if (!convex) {
            out.push_back(f);
            used[i] = 1;
            continue;
        }
        out.push_back(q);
        used[i] = used[best] = 1;
    }
    m.faces = out;
}
} // namespace

namespace {
// The rock from n starting points (the triangle count depends on n and the cut).
Mesh rockFrom(const RockParams& p, int n) {
    std::mt19937 rng(p.seed);
    std::uniform_real_distribution<double> uni(-1.0, 1.0);
    const double rough = p.roughness / 100.0;
    std::vector<Vec3> pts;
    const double golden = kPi * (3 - std::sqrt(5.0));
    for (int i = 0; i < n; ++i) {
        // Evenly spread (Fibonacci sphere), then jittered.
        const double y = 1 - 2 * (i + 0.5) / n;
        const double r = std::sqrt(std::max(0.0, 1 - y * y));
        const double t = golden * i + uni(rng) * rough * 1.2;
        Vec3 q{r * std::cos(t), r * std::sin(t), y};
        q = normalize(q + Vec3{uni(rng), uni(rng), uni(rng)} * (0.35 * rough));
        q = q * (1 - 0.3 * rough * (uni(rng) * 0.5 + 0.5));
        pts.push_back(q);
    }
    // A flat bottom: the hull cut by a level plane (the points where its edges cross the
    // plane are added, the points below dropped). A plane through a convex solid always
    // leaves a proper flat face.
    double cut = -2;
    if (p.flatBottom && p.bottomHeight > 0) {
        const Mesh h = convexHull(pts);
        double lo = 1e9, hi = -1e9;
        for (const Vec3& q : h.verts) {
            lo = std::min(lo, q.z);
            hi = std::max(hi, q.z);
        }
        cut = lo + (hi - lo) * p.bottomHeight / 100.0;
        std::vector<Vec3> kept;
        for (const Vec3& q : h.verts)
            if (q.z >= cut) kept.push_back(q);
        std::set<std::pair<int, int>> done;
        for (const Face& f : h.faces)
            for (int k = 0; k < 3; ++k) {
                const int a = f.v[k], b = f.v[(k + 1) % 3];
                if (!done.insert({std::min(a, b), std::max(a, b)}).second) continue;
                const Vec3 &pa = h.verts[a], &pb = h.verts[b];
                if ((pa.z - cut) * (pb.z - cut) < 0) {
                    const double t = (cut - pa.z) / (pb.z - pa.z);
                    Vec3 x = pa + (pb - pa) * t;
                    x.z = cut;
                    kept.push_back(x);
                }
            }
        pts = kept;
    }
    Mesh m = convexHull(pts);
    // Dents: push some points inward along their normals, but never far enough to turn
    // a face over; bottom points stay on the bottom.
    if (rough > 0) {
        std::vector<Vec3> normals(m.verts.size());
        for (int f = 0; f < (int)m.faces.size(); ++f)
            for (int k = 0; k < 3; ++k) normals[m.faces[f].v[k]] += faceNormal(m, f);
        std::vector<Vec3> pos = m.verts;
        for (size_t v = 0; v < m.verts.size(); ++v) {
            if (p.flatBottom && m.verts[v].z <= cut + 1e-9) continue;
            double amount = rough * 0.18 * (uni(rng) * 0.5 + 0.5);
            for (int tries = 0; tries < 6 && amount > 1e-4; ++tries, amount /= 2) {
                std::vector<Vec3> trial = pos;
                trial[v] = m.verts[v] - normalize(normals[v]) * amount;
                if (!anyFolded(m, trial)) {
                    pos = trial;
                    break;
                }
            }
        }
        m.verts = pos;
    }
    // Smoothing: the round part in one group, the flat bottom flat.
    for (int f = 0; f < (int)m.faces.size(); ++f) {
        bool bottom = p.flatBottom;
        for (int k = 0; k < 3 && bottom; ++k) bottom = m.verts[m.faces[f].v[k]].z <= cut + 1e-9;
        m.faces[f].sg = bottom ? 0 : 1;
    }
    if (p.joinQuads) joinIntoQuads(m);
    // Local frame: u, v across, w up from 0.
    for (Vec3& q : m.verts) q.z = (q.z + 1) / 2;
    return m;
}

int triangles(const Mesh& m) {
    int t = 0;
    for (const Face& f : m.faces) t += f.n - 2;
    return t;
}
} // namespace

Mesh makeRockLocal(const RockParams& in) {
    const RockParams p = clampRock(in);
    // A hull of n points on a sphere has 2n - 4 triangles; a flat bottom adds more, so
    // take the number of starting points that comes closest to the asked count.
    const int most = std::max(6, p.faces / 2 + 2);
    Mesh best;
    int bestDiff = 1 << 30;
    for (int n = most; n >= 5; --n) {
        Mesh m = rockFrom(p, n);
        const int diff = std::abs(triangles(m) - p.faces);
        if (diff < bestDiff && !m.faces.empty()) {
            bestDiff = diff;
            best = std::move(m);
        }
        if (diff == 0 || !p.flatBottom) break;
    }
    return best;
}

} // namespace mazz
