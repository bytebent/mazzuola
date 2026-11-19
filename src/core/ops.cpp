#include "core/ops.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <numeric>

namespace mazz {

// ---------------------------------------------------------------- selection

std::set<int> selectedVerts(const Mesh& m, const Selection& s, SelectMode mode) {
    std::set<int> v;
    switch (mode) {
    case SelectMode::Vertex: v = s.verts; break;
    case SelectMode::Edge:
        for (const Edge& e : s.edges) { v.insert(e.first); v.insert(e.second); }
        break;
    case SelectMode::Face:
    case SelectMode::Piece:
        for (int f : s.faces)
            if (f >= 0 && f < (int)m.faces.size())
                for (int k = 0; k < m.faces[f].n; ++k) v.insert(m.faces[f].v[k]);
        break;
    }
    return v;
}

std::set<int> facesFullySelected(const Mesh& m, const std::set<int>& verts) {
    std::set<int> out;
    for (int f = 0; f < (int)m.faces.size(); ++f) {
        const Face& fc = m.faces[f];
        if (fc.hidden) continue;
        bool all = true;
        for (int k = 0; k < fc.n; ++k) all = all && verts.count(fc.v[k]);
        if (all) out.insert(f);
    }
    return out;
}

Selection convertSelection(const Mesh& m, const Selection& s, SelectMode from, SelectMode to) {
    if (from == to) return s;
    const std::set<int> verts = selectedVerts(m, s, from);
    Selection r;
    switch (to) {
    case SelectMode::Vertex: r.verts = verts; break;
    case SelectMode::Edge:
        for (const Edge& e : m.edges(false))
            if (verts.count(e.first) && verts.count(e.second)) r.edges.insert(e);
        break;
    case SelectMode::Face: r.faces = (from == SelectMode::Piece) ? s.faces : facesFullySelected(m, verts); break;
    case SelectMode::Piece: {
        const std::set<int> base = (from == SelectMode::Face) ? s.faces : facesFullySelected(m, verts);
        const std::vector<int> ids = pieceIds(m);
        std::set<int> pieces;
        for (int f : base) pieces.insert(ids[f]);
        for (int f = 0; f < (int)m.faces.size(); ++f)
            if (pieces.count(ids[f]) && !m.faces[f].hidden) r.faces.insert(f);
        break;
    }
    }
    return r;
}

void cleanSelection(const Mesh& m, Selection& s) {
    std::set<int> visibleVerts;
    for (const Face& f : m.faces)
        if (!f.hidden)
            for (int k = 0; k < f.n; ++k) visibleVerts.insert(f.v[k]);
    for (auto it = s.verts.begin(); it != s.verts.end();) it = visibleVerts.count(*it) ? std::next(it) : s.verts.erase(it);
    const std::vector<Edge> es = m.edges(false);
    const std::set<Edge> live(es.begin(), es.end());
    for (auto it = s.edges.begin(); it != s.edges.end();) it = live.count(*it) ? std::next(it) : s.edges.erase(it);
    for (auto it = s.faces.begin(); it != s.faces.end();)
        it = (*it >= 0 && *it < (int)m.faces.size() && !m.faces[*it].hidden) ? std::next(it) : s.faces.erase(it);
}

// ---------------------------------------------------------------- topology

std::vector<int> pieceIds(const Mesh& m) {
    std::vector<int> parent(m.verts.size());
    std::iota(parent.begin(), parent.end(), 0);
    std::function<int(int)> find = [&](int x) { return parent[x] == x ? x : parent[x] = find(parent[x]); };
    for (const Face& f : m.faces)
        for (int k = 1; k < f.n; ++k) {
            const int a = find(f.v[0]), b = find(f.v[k]);
            if (a != b) parent[a] = b;
        }
    std::vector<int> ids(m.faces.size());
    for (size_t f = 0; f < m.faces.size(); ++f) ids[f] = find(m.faces[f].v[0]);
    return ids;
}

std::set<int> pieceOf(const Mesh& m, int face) {
    std::set<int> out;
    if (face < 0 || face >= (int)m.faces.size()) return out;
    const std::vector<int> ids = pieceIds(m);
    for (int f = 0; f < (int)m.faces.size(); ++f)
        if (ids[f] == ids[face] && !m.faces[f].hidden) out.insert(f);
    return out;
}

namespace {
struct EdgeInfo {
    std::vector<int> faces;
};
std::map<Edge, EdgeInfo> edgeFaces(const Mesh& m) {
    std::map<Edge, EdgeInfo> ef;
    for (int f = 0; f < (int)m.faces.size(); ++f) {
        const Face& fc = m.faces[f];
        if (fc.hidden) continue;
        for (int k = 0; k < fc.n; ++k) ef[makeEdge(fc.v[k], fc.v[(k + 1) % fc.n])].faces.push_back(f);
    }
    return ef;
}
std::map<int, std::vector<Edge>> vertexEdges(const std::map<Edge, EdgeInfo>& ef) {
    std::map<int, std::vector<Edge>> ve;
    for (const auto& [e, info] : ef) {
        ve[e.first].push_back(e);
        ve[e.second].push_back(e);
    }
    return ve;
}
bool faceHasEdge(const Face& f, const Edge& e) {
    for (int k = 0; k < f.n; ++k)
        if (makeEdge(f.v[k], f.v[(k + 1) % f.n]) == e) return true;
    return false;
}
} // namespace

std::set<Edge> edgeLoop(const Mesh& m, const Edge& start) {
    const auto ef = edgeFaces(m);
    const auto ve = vertexEdges(ef);
    std::set<Edge> loop{start};
    if (!ef.count(start)) return loop;
    // Walk from each end.
    for (int dir = 0; dir < 2; ++dir) {
        Edge cur = start;
        int at = dir == 0 ? start.second : start.first;
        for (int guard = 0; guard < 100000; ++guard) {
            auto vit = ve.find(at);
            if (vit == ve.end() || vit->second.size() != 4) break;
            // The next edge at `at` shares no face with the current edge.
            const std::vector<int>& curFaces = ef.at(cur).faces;
            Edge next{-1, -1};
            int candidates = 0;
            for (const Edge& e : vit->second) {
                if (e == cur) continue;
                bool shares = false;
                for (int f : curFaces)
                    if (faceHasEdge(m.faces[f], e)) shares = true;
                if (!shares) { next = e; ++candidates; }
            }
            if (candidates != 1 || loop.count(next)) break;
            loop.insert(next);
            at = next.first == at ? next.second : next.first;
            cur = next;
        }
    }
    return loop;
}

std::set<Edge> edgeRing(const Mesh& m, const Edge& start) {
    const auto ef = edgeFaces(m);
    std::set<Edge> ring{start};
    auto it0 = ef.find(start);
    if (it0 == ef.end()) return ring;
    for (int f0 : it0->second.faces) {
        Edge cur = start;
        int face = f0;
        for (int guard = 0; guard < 100000; ++guard) {
            const Face& fc = m.faces[face];
            if (fc.n != 4) break;
            int k = 0;
            while (k < 4 && makeEdge(fc.v[k], fc.v[(k + 1) % 4]) != cur) ++k;
            if (k == 4) break;
            const Edge opp = makeEdge(fc.v[(k + 2) % 4], fc.v[(k + 3) % 4]);
            if (ring.count(opp)) break;
            ring.insert(opp);
            const std::vector<int>& fs = ef.at(opp).faces;
            int nextFace = -1;
            for (int f : fs)
                if (f != face) nextFace = f;
            if (nextFace < 0) break;
            cur = opp;
            face = nextFace;
        }
    }
    return ring;
}

void growSelection(const Mesh& m, Selection& s, SelectMode mode) {
    const std::set<int> verts = selectedVerts(m, s, mode);
    switch (mode) {
    case SelectMode::Vertex:
        for (const Edge& e : m.edges(false)) {
            if (verts.count(e.first)) s.verts.insert(e.second);
            if (verts.count(e.second)) s.verts.insert(e.first);
        }
        break;
    case SelectMode::Edge:
        for (const Edge& e : m.edges(false))
            if (verts.count(e.first) || verts.count(e.second)) s.edges.insert(e);
        break;
    case SelectMode::Face:
    case SelectMode::Piece:
        for (int f = 0; f < (int)m.faces.size(); ++f) {
            if (m.faces[f].hidden) continue;
            for (int k = 0; k < m.faces[f].n; ++k)
                if (verts.count(m.faces[f].v[k])) { s.faces.insert(f); break; }
        }
        break;
    }
}

void shrinkSelection(const Mesh& m, Selection& s, SelectMode mode) {
    // Remove anything touching an unselected neighbour.
    const std::set<int> verts = selectedVerts(m, s, mode);
    std::set<int> border; // selected points next to an unselected point / face
    switch (mode) {
    case SelectMode::Vertex: {
        for (const Edge& e : m.edges(false)) {
            if (verts.count(e.first) && !verts.count(e.second)) border.insert(e.first);
            if (verts.count(e.second) && !verts.count(e.first)) border.insert(e.second);
        }
        for (int v : border) s.verts.erase(v);
        break;
    }
    case SelectMode::Edge: {
        for (const Edge& e : m.edges(false))
            if (!s.edges.count(e)) {
                if (verts.count(e.first)) border.insert(e.first);
                if (verts.count(e.second)) border.insert(e.second);
            }
        for (auto it = s.edges.begin(); it != s.edges.end();)
            it = (border.count(it->first) || border.count(it->second)) ? s.edges.erase(it) : std::next(it);
        break;
    }
    case SelectMode::Face:
    case SelectMode::Piece: {
        for (int f = 0; f < (int)m.faces.size(); ++f)
            if (!s.faces.count(f) && !m.faces[f].hidden)
                for (int k = 0; k < m.faces[f].n; ++k) border.insert(m.faces[f].v[k]);
        for (auto it = s.faces.begin(); it != s.faces.end();) {
            bool touches = false;
            for (int k = 0; k < m.faces[*it].n; ++k) touches = touches || border.count(m.faces[*it].v[k]);
            it = touches ? s.faces.erase(it) : std::next(it);
        }
        break;
    }
    }
}

std::set<int> similarFaces(const Mesh& m, const std::set<int>& faces, Similar how, double maxAngleDeg) {
    std::set<int> out;
    const double cosMax = std::cos(radians(maxAngleDeg));
    for (int f = 0; f < (int)m.faces.size(); ++f) {
        if (m.faces[f].hidden) continue;
        for (int g : faces) {
            if (g < 0 || g >= (int)m.faces.size()) continue;
            bool like = false;
            switch (how) {
            case Similar::Texture: like = m.faces[f].tex == m.faces[g].tex; break;
            case Similar::SmoothingGroup: like = m.faces[f].sg == m.faces[g].sg; break;
            case Similar::Direction: like = dot(m.faceNormal(f), m.faceNormal(g)) >= cosMax; break;
            }
            if (like) { out.insert(f); break; }
        }
    }
    return out;
}

// ---------------------------------------------------------------- transforms

Vec3 Affine::apply(const Vec3& p) const {
    return {m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + t.x, m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + t.y,
            m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z + t.z};
}

Affine Affine::then(const Affine& n) const {
    Affine r;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0;
            for (int k = 0; k < 3; ++k) s += n.m[i][k] * m[k][j];
            r.m[i][j] = s;
        }
    r.t = n.apply(t);
    return r;
}

double Affine::det() const {
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
           m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

Affine translation(const Vec3& d) {
    Affine a;
    a.t = d;
    return a;
}

namespace {
Affine rotAxis(int axis, double deg) {
    // Exact for multiples of 90 degrees (no rounding drift on typed right angles).
    double c, s;
    const double q = deg / 90.0;
    if (std::fabs(q - std::round(q)) < 1e-12) {
        const int k = ((int)std::lround(q) % 4 + 4) % 4;
        const double cs[4] = {1, 0, -1, 0}, sn[4] = {0, 1, 0, -1};
        c = cs[k];
        s = sn[k];
    } else {
        c = std::cos(radians(deg));
        s = std::sin(radians(deg));
    }
    Affine a;
    const int i = (axis + 1) % 3, j = (axis + 2) % 3;
    a.m[i][i] = c;
    a.m[i][j] = -s;
    a.m[j][i] = s;
    a.m[j][j] = c;
    return a;
}
} // namespace

Affine rotationXYZ(const Vec3& d) { return rotAxis(0, d.x).then(rotAxis(1, d.y)).then(rotAxis(2, d.z)); }

Affine scaling(const Vec3& f) {
    Affine a;
    a.m[0][0] = f.x;
    a.m[1][1] = f.y;
    a.m[2][2] = f.z;
    return a;
}

Affine about(const Affine& a, const Vec3& pivot) { return translation(-pivot).then(a).then(translation(pivot)); }

Affine mirrorAxis(int axis, const Vec3& pivot) {
    Vec3 f{1, 1, 1};
    f[axis] = -1;
    return about(scaling(f), pivot);
}

namespace {
void flipWinding(Face& f) {
    std::reverse(f.v.begin(), f.v.begin() + f.n);
    std::reverse(f.uv.begin(), f.uv.begin() + f.n);
    std::reverse(f.col.begin(), f.col.begin() + f.n);
    // Corners (a,b,c,d) become (d,c,b,a): the old 0-2 split (a-c) is now corners 3-1,
    // so the same diagonal needs the other setting.
    if (f.n == 4) f.diag = !f.diag;
}
} // namespace

void transformPoints(Mesh& m, const std::set<int>& verts, const Affine& a) {
    for (int v : verts)
        if (v >= 0 && v < (int)m.verts.size()) m.verts[v] = a.apply(m.verts[v]);
    if (a.det() < 0)
        for (int f : facesFullySelected(m, verts)) flipWinding(m.faces[f]);
}

void moveAlongNormal(Mesh& m, const std::set<int>& faces, double distance) {
    Vec3 n;
    std::set<int> verts;
    for (int f : faces) {
        n += m.faceNormal(f) * m.faceArea(f);
        for (int k = 0; k < m.faces[f].n; ++k) verts.insert(m.faces[f].v[k]);
    }
    if (length(n) < 1e-12) return;
    transformPoints(m, verts, translation(normalize(n) * distance));
}

std::set<int> duplicateFaces(Mesh& m, const std::set<int>& faces) {
    std::map<int, int> remap;
    std::set<int> out;
    const std::vector<Face> src = m.faces;
    for (int f : faces) {
        if (f < 0 || f >= (int)src.size()) continue;
        Face nf = src[f];
        for (int k = 0; k < nf.n; ++k) {
            auto it = remap.find(nf.v[k]);
            if (it == remap.end()) it = remap.emplace(nf.v[k], m.addVertex(m.verts[nf.v[k]])).first;
            nf.v[k] = it->second;
        }
        nf.hidden = false;
        m.faces.push_back(nf);
        out.insert((int)m.faces.size() - 1);
    }
    return out;
}

std::set<int> cloneFaces(Mesh& m, const std::set<int>& faces, const CloneParams& p) {
    std::set<int> all;
    const int count = std::max(0, p.count);
    const Vec3 stepOffset = p.offsetIsTotal && count > 0 ? p.offset / count : p.offset;
    const Vec3 sf{p.scale.x / 100.0, p.scale.y / 100.0, p.scale.z / 100.0};
    Vec3 offsetSum;
    Vec3 scalePow{1, 1, 1};
    for (int k = 1; k <= count; ++k) {
        // Offset for this step: plain k * step, or a geometric series with the scale.
        if (p.scaleOffset) {
            offsetSum += Vec3{stepOffset.x * scalePow.x, stepOffset.y * scalePow.y, stepOffset.z * scalePow.z};
        } else {
            offsetSum = stepOffset * k;
        }
        scalePow = {scalePow.x * sf.x, scalePow.y * sf.y, scalePow.z * sf.z};
        const Affine rs = scaling(scalePow).then(rotationXYZ(p.rotate * k));
        const Affine a = about(rs, p.pivot).then(translation(offsetSum));
        const std::set<int> copy = duplicateFaces(m, faces);
        std::set<int> verts;
        for (int f : copy)
            for (int c = 0; c < m.faces[f].n; ++c) verts.insert(m.faces[f].v[c]);
        transformPoints(m, verts, a);
        all.insert(copy.begin(), copy.end());
    }
    return all;
}

std::set<int> mirrorFaces(Mesh& m, const std::set<int>& faces, int axis, const Vec3& pivot, bool copy) {
    const std::set<int> target = copy ? duplicateFaces(m, faces) : faces;
    std::set<int> verts;
    for (int f : target)
        for (int k = 0; k < m.faces[f].n; ++k) verts.insert(m.faces[f].v[k]);
    const Affine a = mirrorAxis(axis, pivot);
    for (int v : verts) m.verts[v] = a.apply(m.verts[v]);
    for (int f : target) flipWinding(m.faces[f]);
    return target;
}

void flattenPoints(Mesh& m, const std::set<int>& verts, int axis, double value) {
    for (int v : verts)
        if (v >= 0 && v < (int)m.verts.size()) m.verts[v][axis] = value;
}

void snapPoints(Mesh& m, const std::set<int>& verts, double grid) {
    if (grid <= 0) return;
    for (int v : verts)
        if (v >= 0 && v < (int)m.verts.size())
            for (int a = 0; a < 3; ++a) m.verts[v][a] = std::round(m.verts[v][a] / grid) * grid;
}

std::set<int> facesOfSelection(const Mesh& m, const Selection& s, SelectMode mode) {
    std::set<int> out;
    if (mode == SelectMode::Face || mode == SelectMode::Piece) return s.faces;
    for (int f = 0; f < (int)m.faces.size(); ++f) {
        const Face& fc = m.faces[f];
        for (int k = 0; k < fc.n; ++k) {
            if (mode == SelectMode::Vertex && s.verts.count(fc.v[k])) out.insert(f);
            if (mode == SelectMode::Edge && s.edges.count(makeEdge(fc.v[k], fc.v[(k + 1) % fc.n]))) out.insert(f);
        }
    }
    return out;
}

int deleteSelection(Mesh& m, const Selection& s, SelectMode mode) {
    const std::set<int> faces = facesOfSelection(m, s, mode);
    std::vector<bool> remove(m.faces.size(), false);
    for (int f : faces)
        if (f >= 0 && f < (int)m.faces.size()) remove[f] = true;
    m.removeFaces(remove);
    return (int)faces.size();
}

void setHidden(Mesh& m, const std::set<int>& faces, bool hidden) {
    for (int f : faces)
        if (f >= 0 && f < (int)m.faces.size()) m.faces[f].hidden = hidden;
}

int joinTrianglesIntoQuads(Mesh& m, double maxAngleDeg, const std::set<int>* only) {
    const double cosMax = std::cos(radians(maxAngleDeg));
    // Candidate pairs, best (flattest) first.
    struct Pair {
        int a, b;
        double score;
        std::array<int, 4> quad;
    };
    std::vector<Pair> pairs;
    std::map<Edge, std::vector<int>> ef;
    for (int f = 0; f < (int)m.faces.size(); ++f) {
        if (m.faces[f].n != 3 || (only && !only->count(f))) continue;
        for (int k = 0; k < 3; ++k) ef[makeEdge(m.faces[f].v[k], m.faces[f].v[(k + 1) % 3])].push_back(f);
    }
    for (const auto& [e, fs] : ef) {
        if (fs.size() != 2) continue;
        const int a = fs[0], b = fs[1];
        if (m.faces[a].sg != m.faces[b].sg || m.faces[a].tex != m.faces[b].tex || m.faces[a].part != m.faces[b].part) continue;
        const double c = dot(m.faceNormal(a), m.faceNormal(b));
        if (c < cosMax) continue;
        // Quad corners: a's corners starting after the shared edge, then b's far corner.
        const Face& fa = m.faces[a];
        int k = 0;
        while (makeEdge(fa.v[k], fa.v[(k + 1) % 3]) != e) ++k;
        const int p0 = fa.v[k], p1 = fa.v[(k + 1) % 3], p2 = fa.v[(k + 2) % 3];
        int far = -1;
        for (int j = 0; j < 3; ++j)
            if (m.faces[b].v[j] != p0 && m.faces[b].v[j] != p1) far = m.faces[b].v[j];
        // In a's winding the shared edge runs p0 -> p1; the quad is p1, p2, p0, far.
        const std::array<int, 4> q{p1, p2, p0, far};
        // Convex: every corner turns the same way as the normal.
        const Vec3 n = normalize(m.faceNormal(a) + m.faceNormal(b));
        bool convex = true;
        for (int i = 0; i < 4; ++i) {
            const Vec3 e1 = m.verts[q[(i + 1) % 4]] - m.verts[q[i]];
            const Vec3 e2 = m.verts[q[(i + 2) % 4]] - m.verts[q[(i + 1) % 4]];
            if (dot(cross(e1, e2), n) <= 1e-12) convex = false;
        }
        if (convex) pairs.push_back({a, b, c, q});
    }
    std::sort(pairs.begin(), pairs.end(), [](const Pair& x, const Pair& y) {
        if (x.score != y.score) return x.score > y.score;
        return std::make_pair(x.a, x.b) < std::make_pair(y.a, y.b);
    });
    std::vector<bool> used(m.faces.size(), false), remove(m.faces.size(), false);
    int made = 0;
    for (const Pair& p : pairs) {
        if (used[p.a] || used[p.b]) continue;
        used[p.a] = used[p.b] = true;
        Face q = m.faces[p.a];
        q.n = 4;
        q.v = p.quad;
        q.diag = false; // split 0-2: corners p1 and p0, the old shared edge
        m.faces[p.a] = q;
        remove[p.b] = true;
        ++made;
    }
    if (made) m.removeFaces(remove);
    return made;
}

std::vector<std::array<Vec3, 4>> cornerNormals(const Mesh& m) {
    std::vector<Vec3> fn(m.faces.size());
    std::vector<double> fa(m.faces.size());
    for (int f = 0; f < (int)m.faces.size(); ++f) {
        fn[f] = m.faceNormal(f);
        fa[f] = m.faceArea(f);
    }
    // (vertex, group) -> summed normal
    std::map<std::pair<int, int>, Vec3> sum;
    for (int f = 0; f < (int)m.faces.size(); ++f) {
        const Face& fc = m.faces[f];
        if (fc.sg == 0) continue;
        for (int k = 0; k < fc.n; ++k) sum[{fc.v[k], fc.sg}] += fn[f] * fa[f];
    }
    std::vector<std::array<Vec3, 4>> out(m.faces.size());
    for (int f = 0; f < (int)m.faces.size(); ++f) {
        const Face& fc = m.faces[f];
        for (int k = 0; k < fc.n; ++k) {
            if (fc.sg == 0) out[f][k] = fn[f];
            else {
                const Vec3 s = sum[{fc.v[k], fc.sg}];
                out[f][k] = length(s) > 1e-12 ? normalize(s) : fn[f];
            }
        }
    }
    return out;
}

} // namespace mazz
