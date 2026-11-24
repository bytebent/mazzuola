#include "core/mirror_ops.hpp"
#include "core/clean_ops.hpp"
#include "core/draw_ops.hpp"
#include "core/model_ops.hpp"
#include "core/ops.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

namespace mazz {

namespace {

using Key = std::tuple<long long, long long, long long>;
Key keyOf(const Vec3& p) { return {std::llround(p.x * 1000), std::llround(p.y * 1000), std::llround(p.z * 1000)}; }

bool fail(std::string* why, const char* w) {
    if (why) *why = w;
    return false;
}

} // namespace

Vec3 mirrored(const Vec3& p, int axis) {
    Vec3 q = p;
    q[axis] = -q[axis];
    return q;
}

std::vector<int> twinPoints(const Mesh& m, int axis) {
    const int n = (int)m.verts.size();
    std::map<Key, std::vector<int>> at;
    for (int i = 0; i < n; ++i) at[keyOf(m.verts[i])].push_back(i);
    std::vector<int> twin(n, -1);
    for (const auto& [k, list] : at) {
        (void)k;
        const Vec3 p = m.verts[list.front()];
        if (std::abs(p[axis]) <= kOnPlane) {
            for (int i : list) twin[i] = i;
            continue;
        }
        auto it = at.find(keyOf(mirrored(p, axis)));
        if (it == at.end()) continue;
        for (size_t r = 0; r < list.size() && r < it->second.size(); ++r) twin[list[r]] = it->second[r];
    }
    return twin;
}

int unpairedPoints(const Mesh& m, int axis) {
    const auto twin = twinPoints(m, axis);
    std::vector<bool> used(m.verts.size(), false);
    for (const Face& f : m.faces)
        for (int k = 0; k < f.n; ++k) used[f.v[k]] = true;
    int n = 0;
    for (int i = 0; i < (int)twin.size(); ++i) n += used[i] && twin[i] < 0 ? 1 : 0;
    return n;
}

std::vector<int> twinFaces(const Mesh& m, const std::vector<int>& twins) {
    std::map<std::vector<int>, int> byCorners;
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        std::vector<int> c(m.faces[fi].v, m.faces[fi].v + m.faces[fi].n);
        std::sort(c.begin(), c.end());
        byCorners[c] = fi;
    }
    std::vector<int> out(m.faces.size(), -1);
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        std::vector<int> c;
        bool ok = true;
        for (int k = 0; k < m.faces[fi].n && ok; ++k) {
            const int t = twins[m.faces[fi].v[k]];
            ok = t >= 0;
            c.push_back(t);
        }
        if (!ok) continue;
        std::sort(c.begin(), c.end());
        auto it = byCorners.find(c);
        if (it != byCorners.end()) out[fi] = it->second;
    }
    return out;
}

Selection withTwins(const Mesh& m, const Selection& s, int axis) {
    if (axis < 0 || axis > 2 || s.empty()) return s;
    const auto twins = twinPoints(m, axis);
    Selection out = s;
    for (int v : s.verts)
        if (v >= 0 && v < (int)twins.size() && twins[v] >= 0) out.verts.insert(twins[v]);
    if (!s.edges.empty()) {
        std::set<Edge> exist;
        for (const Face& f : m.faces)
            for (int k = 0; k < f.n; ++k) exist.insert(makeEdge(f.v[k], f.v[(k + 1) % f.n]));
        for (const Edge& ed : s.edges) {
            if (ed.first >= (int)twins.size() || ed.second >= (int)twins.size()) continue;
            const int a = twins[ed.first], b = twins[ed.second];
            if (a >= 0 && b >= 0 && exist.count(makeEdge(a, b))) out.edges.insert(makeEdge(a, b));
        }
    }
    if (!s.faces.empty()) {
        const auto tf = twinFaces(m, twins);
        for (int f : s.faces)
            if (f >= 0 && f < (int)tf.size() && tf[f] >= 0) out.faces.insert(tf[f]);
    }
    return out;
}

void mirrorFix(MeshEdit& e, int axis, int side, const std::vector<int>& twins) {
    if (axis < 0 || axis > 2) return;
    const int n = std::min((int)twins.size(), e.oldVertCount());
    for (int a = 0; a < n; ++a) {
        Vec3 before = e.mesh().verts[a];
        const bool moved = e.vertChanged(a, &before);
        const double c = before[axis];
        if (std::abs(c) <= kOnPlane) {
            // On the plane: stays on it.
            if (moved && e.mesh().verts[a][axis] != 0) {
                Vec3 q = e.mesh().verts[a];
                q[axis] = 0;
                e.setVert(a, q);
            }
            continue;
        }
        if (c * side < 0) continue; // the other side follows this one
        const int b = twins[a];
        if (b < 0 || b == a || b >= n) continue;
        if (!moved && !e.vertChanged(b)) continue;
        const Vec3 want = mirrored(e.mesh().verts[a], axis);
        if (length(e.mesh().verts[b] - want) > 1e-9) e.setVert(b, want);
    }
}

// ---------------------------------------------------------------- make symmetric

bool makeSymmetric(MeshEdit& e, int axis, int keepSide, std::string* why) {
    if (axis < 0 || axis > 2 || (keepSide != 1 && keepSide != -1)) return fail(why, "pick a plane and a side");
    const Mesh& m = e.mesh();
    const bool closed = isClosed(m);
    auto side = [&](int v) {
        const double c = m.verts[v][axis] * keepSide;
        return c > kOnPlane ? 1 : c < -kOnPlane ? -1 : 0;
    };
    // 1. Points where edges cross the plane.
    std::map<Edge, int> cross;
    const int nf = (int)m.faces.size();
    for (int fi = 0; fi < nf; ++fi) {
        const Face f = m.faces[fi];
        for (int k = 0; k < f.n; ++k) {
            const int a = f.v[k], b = f.v[(k + 1) % f.n];
            if (side(a) * side(b) >= 0) continue;
            const Edge ed = makeEdge(a, b);
            if (cross.count(ed)) continue;
            const Vec3 A = m.verts[a], B = m.verts[b];
            const double t = A[axis] / (A[axis] - B[axis]);
            Vec3 p = A + (B - A) * t;
            p[axis] = 0;
            cross[ed] = e.addVert(p);
        }
    }
    // 2. Each face: the part on the kept side (with the crossing points), or nothing.
    std::set<int> gone;
    std::set<int> kept;
    for (int fi = 0; fi < nf; ++fi) {
        const Face f = e.mesh().faces[fi];
        std::vector<int> poly;
        for (int k = 0; k < f.n; ++k) {
            const int a = f.v[k], b = f.v[(k + 1) % f.n];
            poly.push_back(a);
            auto it = cross.find(makeEdge(a, b));
            if (it != cross.end()) poly.push_back(it->second);
        }
        int pos = 0, neg = 0;
        std::vector<int> sides;
        for (int v : poly) {
            const int sv = side(v); // crossing points lie on the plane: 0
            sides.push_back(sv);
            pos += sv > 0, neg += sv < 0;
        }
        if (pos == 0) { // all on the other side, or lying in the plane
            gone.insert(fi);
            continue;
        }
        if (neg == 0 && poly.size() == (size_t)f.n) {
            kept.insert(fi);
            continue;
        }
        // Across the plane: keep the run on this side (and the points on the plane).
        std::vector<int> part;
        int runs = 0;
        for (size_t i = 0; i < poly.size(); ++i) {
            if (sides[i] >= 0) part.push_back(poly[i]);
            if (sides[i] < 0 && sides[(i + 1) % poly.size()] >= 0) ++runs;
        }
        if (runs > 1) return fail(why, "a face crosses the plane more than once; cut it first");
        Vec3 n = faceNormal(m, fi);
        std::vector<std::vector<int>> pieces;
        if (part.size() <= 4) pieces.push_back(part);
        else pieces = splitPolygon(e.mesh(), part, n);
        if (pieces.empty() || part.size() < 3) return fail(why, "a face across the plane can't be cut there");
        for (size_t i = 0; i < pieces.size(); ++i) {
            Face g = f;
            g.n = (int)pieces[i].size();
            for (int k = 0; k < g.n; ++k) g.v[k] = pieces[i][k];
            g.diag = false;
            if (i == 0) {
                e.setFace(fi, g);
                kept.insert(fi);
            } else {
                kept.insert(e.addFace(g));
            }
        }
    }
    // (An edge crossing the plane has both its faces across it: both were cut above.)
    if (kept.empty()) return fail(why, "nothing is on that side");
    std::vector<int> fmap;
    e.deleteFaces(gone, &fmap);
    // 3. The mirror copy, welded to this half along the plane.
    Selection half;
    half.mode = SelectMode::Face;
    for (int fi = 0; fi < (int)e.mesh().faces.size(); ++fi) half.faces.insert(fi);
    mirrorClone(e, half, axis);
    std::set<int> onPlane;
    for (int v = 0; v < (int)e.mesh().verts.size(); ++v)
        if (std::abs(e.mesh().verts[v][axis]) <= kOnPlane) onPlane.insert(v);
    if (!onPlane.empty()) weldByDistance(e, onPlane, kOnPlane);
    if (closed && !isClosed(e.mesh())) return fail(why, "the halves don't meet along the plane; it would open the model");
    return true;
}

} // namespace mazz
