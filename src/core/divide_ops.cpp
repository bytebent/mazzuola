#include "core/divide_ops.hpp"
#include "core/draw_ops.hpp"
#include "core/numbers.hpp"
#include <algorithm>
#include <cmath>
#include <map>

namespace mazz {

namespace {

constexpr double kMinArea = 1e-3; // cm²

double polyArea(const Mesh& m, const std::vector<int>& p) {
    Vec3 s{0, 0, 0};
    for (size_t k = 1; k + 1 < p.size(); ++k) s = s + cross(m.verts[p[k]] - m.verts[p[0]], m.verts[p[k + 1]] - m.verts[p[0]]);
    return length(s) / 2;
}

Face faceFrom(const Face& like, const std::vector<int>& vs) {
    Face f;
    f.tex = like.tex;
    f.group = like.group;
    f.sg = like.sg;
    f.twoSided = like.twoSided;
    f.n = (int)vs.size();
    for (int k = 0; k < f.n; ++k) f.v[k] = vs[k];
    return f;
}

// Replaces face fi by pieces (first in its place). False if a piece has no area.
bool replaceWith(MeshEdit& e, int fi, const std::vector<std::vector<int>>& pieces, std::vector<int>* made = nullptr) {
    if (pieces.empty()) return false;
    for (const auto& p : pieces)
        if (p.size() < 3 || p.size() > 4 || polyArea(e.mesh(), p) < kMinArea) return false;
    const Face like = e.mesh().faces[fi];
    for (size_t i = 0; i < pieces.size(); ++i) {
        const Face f = faceFrom(like, pieces[i]);
        int idx = (int)fi;
        if (i == 0) e.setFace(fi, f);
        else idx = e.addFace(f);
        if (made) made->push_back(idx);
    }
    return true;
}

// New points on edges (key: smaller point first; points in order from it) go into
// every face using those edges, except `skip`; those faces are re-split into 3- and
// 4-corner faces.
bool insertOnEdges(MeshEdit& e, const std::map<std::pair<int, int>, std::vector<int>>& pts, const std::set<int>& skip) {
    const int count = (int)e.mesh().faces.size();
    for (int fi = 0; fi < count; ++fi) {
        if (skip.count(fi)) continue;
        const Face f = e.mesh().faces[fi];
        std::vector<int> loop;
        bool changed = false;
        for (int k = 0; k < f.n; ++k) {
            const int a = f.v[k], b = f.v[(k + 1) % f.n];
            loop.push_back(a);
            auto it = pts.find({std::min(a, b), std::max(a, b)});
            if (it == pts.end() || it->second.empty()) continue;
            changed = true;
            if (a < b) loop.insert(loop.end(), it->second.begin(), it->second.end());
            else loop.insert(loop.end(), it->second.rbegin(), it->second.rend());
        }
        if (!changed) continue;
        const auto pieces = splitPolygon(e.mesh(), loop, faceNormal(e.mesh(), fi));
        if (!replaceWith(e, fi, pieces)) return false;
    }
    return true;
}

std::pair<int, int> key(int a, int b) { return {std::min(a, b), std::max(a, b)}; }

// n - 1 points evenly along a -> b, in order from the smaller index (shared by both faces).
std::vector<int> pointsAlong(MeshEdit& e, std::map<std::pair<int, int>, std::vector<int>>& cache, int a, int b, int n,
                             bool& clash) {
    auto k = key(a, b);
    auto it = cache.find(k);
    if (it != cache.end()) {
        if ((int)it->second.size() != n - 1) clash = true;
        std::vector<int> v = it->second;
        if (a > b) std::reverse(v.begin(), v.end());
        return v;
    }
    const Vec3 pa = e.mesh().verts[k.first], pb = e.mesh().verts[k.second];
    std::vector<int> v;
    for (int i = 1; i < n; ++i) v.push_back(e.addVert(pa + (pb - pa) * ((double)i / n)));
    cache[k] = v;
    if (a > b) std::reverse(v.begin(), v.end());
    return v;
}

} // namespace

// ---------------------------------------------------------------- knife

bool knife(MeshEdit& e, const std::vector<Vec3>& pts, std::string* why) {
    auto fail = [&](const std::string& w) {
        if (why) *why = w;
        return false;
    };
    if (pts.size() < 2) return fail("click at least two points");
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        const Vec3 a = pts[i], b = pts[i + 1];
        // The face both lie on (on its edges or corners), crossing its inside.
        int face = -1;
        const Mesh& m = e.mesh();
        for (int fi = 0; fi < (int)m.faces.size() && face < 0; ++fi) {
            const FacePlace pa = placeOnFace(m, fi, a), pb = placeOnFace(m, fi, b);
            if (pa == FacePlace::Outside || pa == FacePlace::Inside || pb == FacePlace::Outside || pb == FacePlace::Inside)
                continue;
            if (placeOnFace(m, fi, (a + b) * 0.5) == FacePlace::Inside) face = fi;
        }
        if (face < 0) {
            // Along an existing edge: nothing to cut there.
            bool along = false;
            for (int fi = 0; fi < (int)m.faces.size() && !along; ++fi)
                along = placeOnFace(m, fi, a) != FacePlace::Outside && placeOnFace(m, fi, b) != FacePlace::Outside &&
                        placeOnFace(m, fi, (a + b) * 0.5) != FacePlace::Outside &&
                        placeOnFace(m, fi, (a + b) * 0.5) != FacePlace::Inside;
            if (along) continue;
            return fail("each two points in a row must be on edges of the same face");
        }
        std::string w;
        if (!cutShape(e, face, {a, b}, false, nullptr, &w, false)) return fail(w);
    }
    return true;
}

// ---------------------------------------------------------------- loop cut

LoopRing loopRing(const Mesh& m, const Edge& start) {
    LoopRing r;
    std::map<std::pair<int, int>, int> owner;
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Face& f = m.faces[fi];
        for (int k = 0; k < f.n; ++k) owner[{f.v[k], f.v[(k + 1) % f.n]}] = fi;
    }
    // A quad with the edge; in it the edge runs s1 -> s2.
    int f0 = -1, s1 = -1, s2 = -1;
    for (auto d : {std::make_pair(start.first, start.second), std::make_pair(start.second, start.first)}) {
        auto it = owner.find(d);
        if (it != owner.end() && m.faces[it->second].n == 4) {
            f0 = it->second;
            s1 = d.first;
            s2 = d.second;
            break;
        }
    }
    if (f0 < 0) return r;
    // Across a quad from rail (p, q) entered as p -> q in it: the opposite rail.
    auto across = [&](int fi, int p, int q, bool forwardWinding) {
        const Face& f = m.faces[fi];
        int k = 0;
        while (k < 4 && f.v[k] != (forwardWinding ? p : q)) ++k;
        // forward: f = p, q, c, d -> rail (d, c); reversed (f has q -> p): f = q, p, c, d -> rail (c, d)
        const int c = f.v[(k + 2) % 4], d = f.v[(k + 3) % 4];
        return forwardWinding ? std::make_pair(d, c) : std::make_pair(c, d);
    };
    // Forward from f0.
    std::vector<std::pair<int, int>> fwdRails = {{s1, s2}};
    std::vector<int> fwdFaces;
    std::set<int> seen;
    int fi = f0;
    std::pair<int, int> rail{s1, s2};
    bool forward = true; // the face holds rail.first -> rail.second
    while (true) {
        if (seen.count(fi)) break;
        seen.insert(fi);
        fwdFaces.push_back(fi);
        const auto next = across(fi, rail.first, rail.second, forward);
        fwdRails.push_back(next);
        if (next == fwdRails.front()) {
            r.closed = true;
            break;
        }
        // The face beyond the next rail.
        int beyond = -1;
        bool fw = true;
        auto it = owner.find({next.first, next.second});
        if (it != owner.end() && it->second != fi) beyond = it->second, fw = true;
        else {
            it = owner.find({next.second, next.first});
            if (it != owner.end() && it->second != fi) beyond = it->second, fw = false;
        }
        if (beyond < 0 || m.faces[beyond].n != 4 || seen.count(beyond)) {
            if (beyond >= 0 && seen.count(beyond) && beyond == f0) r.closed = true;
            break;
        }
        fi = beyond;
        rail = next;
        forward = fw;
    }
    if (r.closed) {
        fwdRails.pop_back(); // the first rail again
        r.rails = fwdRails;
        r.faces = fwdFaces;
        return r;
    }
    // Backward from the start rail, through the face on its other side.
    std::vector<std::pair<int, int>> backRails;
    std::vector<int> backFaces;
    rail = {s1, s2};
    int prev = f0;
    while (true) {
        int beyond = -1;
        bool fw = true;
        auto it = owner.find({rail.first, rail.second});
        if (it != owner.end() && it->second != prev && !seen.count(it->second)) beyond = it->second, fw = true;
        else {
            it = owner.find({rail.second, rail.first});
            if (it != owner.end() && it->second != prev && !seen.count(it->second)) beyond = it->second, fw = false;
        }
        if (beyond < 0 || m.faces[beyond].n != 4) break;
        seen.insert(beyond);
        backFaces.push_back(beyond);
        rail = across(beyond, rail.first, rail.second, fw);
        backRails.push_back(rail);
        prev = beyond;
    }
    // Rails in order: back rails reversed, then the forward ones.
    for (auto it = backRails.rbegin(); it != backRails.rend(); ++it) r.rails.push_back(*it);
    for (auto it = backFaces.rbegin(); it != backFaces.rend(); ++it) r.faces.push_back(*it);
    r.rails.insert(r.rails.end(), fwdRails.begin(), fwdRails.end());
    r.faces.insert(r.faces.end(), fwdFaces.begin(), fwdFaces.end());
    return r;
}

namespace {
std::vector<double> cutFractions(int cuts, double position) {
    std::vector<double> f;
    if (cuts == 1) f.push_back(std::clamp(position, 0.01, 0.99));
    else
        for (int j = 1; j <= cuts; ++j) f.push_back((double)j / (cuts + 1));
    return f;
}
} // namespace

std::vector<std::pair<Vec3, Vec3>> loopCutLines(const Mesh& m, const LoopRing& r, int cuts, double position) {
    std::vector<std::pair<Vec3, Vec3>> out;
    if (!r.valid() || cuts < 1) return out;
    const auto fr = cutFractions(cuts, position);
    const size_t n = r.rails.size();
    for (size_t i = 0; i < r.faces.size(); ++i) {
        const auto& a = r.rails[i];
        const auto& b = r.rails[(i + 1) % n];
        for (double f : fr) {
            const Vec3 p = m.verts[a.first] + (m.verts[a.second] - m.verts[a.first]) * f;
            const Vec3 q = m.verts[b.first] + (m.verts[b.second] - m.verts[b.first]) * f;
            out.push_back({p, q});
        }
    }
    return out;
}

bool loopCut(MeshEdit& e, const LoopRing& r, int cuts, double position, std::set<Edge>* newEdges) {
    if (!r.valid() || cuts < 1) return false;
    const auto fr = cutFractions(cuts, position);
    const size_t n = r.rails.size();
    // New points on every rail (from side 1), shared by the faces on both sides.
    std::vector<std::vector<int>> railPts(n);
    for (size_t i = 0; i < n; ++i) {
        const Vec3 a = e.mesh().verts[r.rails[i].first], b = e.mesh().verts[r.rails[i].second];
        for (double f : fr) railPts[i].push_back(e.addVert(a + (b - a) * f));
    }
    std::set<int> ring(r.faces.begin(), r.faces.end());
    // Ends of an open ring: the faces beyond the end rails get the new points.
    std::map<std::pair<int, int>, std::vector<int>> ends;
    if (!r.closed)
        for (size_t i : {(size_t)0, n - 1}) {
            const auto& rail = r.rails[i];
            std::vector<int> pts = railPts[i];
            if (rail.first > rail.second) std::reverse(pts.begin(), pts.end());
            ends[key(rail.first, rail.second)] = pts;
        }
    for (size_t i = 0; i < r.faces.size(); ++i) {
        const int fi = r.faces[i];
        const auto& a = r.rails[i];
        const auto& b = r.rails[(i + 1) % n];
        std::vector<int> P = {a.first}, Q = {b.first};
        P.insert(P.end(), railPts[i].begin(), railPts[i].end());
        Q.insert(Q.end(), railPts[(i + 1) % n].begin(), railPts[(i + 1) % n].end());
        P.push_back(a.second);
        Q.push_back(b.second);
        // Does the face run a.first -> a.second? Else the strips turn the other way.
        const Face& f = e.mesh().faces[fi];
        bool forward = false;
        for (int k = 0; k < 4; ++k)
            if (f.v[k] == a.first && f.v[(k + 1) % 4] == a.second) forward = true;
        std::vector<std::vector<int>> pieces;
        for (size_t j = 0; j + 1 < P.size(); ++j) {
            std::vector<int> q = {P[j], P[j + 1], Q[j + 1], Q[j]};
            if (!forward) std::reverse(q.begin(), q.end());
            pieces.push_back(q);
        }
        if (!replaceWith(e, fi, pieces)) return false;
        if (newEdges)
            for (size_t j = 1; j + 1 < P.size(); ++j) newEdges->insert(makeEdge(P[j], Q[j]));
    }
    if (!ends.empty() && !insertOnEdges(e, ends, ring)) return false;
    return true;
}

// ---------------------------------------------------------------- subdivide, divide, poke, connect

bool subdivide(MeshEdit& e, const std::set<int>& faces, int n, int m, std::set<int>* out, std::string* why) {
    if (faces.empty() || n < 1 || m < 1 || (n == 1 && m == 1)) return false;
    std::map<std::pair<int, int>, std::vector<int>> cache;
    bool clash = false;
    std::set<int> made;
    for (int fi : faces) {
        const Face f = e.mesh().faces[fi];
        std::vector<std::vector<int>> pieces;
        if (f.n == 4) {
            const int a = f.v[0], b = f.v[1], c = f.v[2], d = f.v[3];
            // Grid point (i along a->b, j along a->d).
            std::vector<std::vector<int>> G(n + 1, std::vector<int>(m + 1, -1));
            G[0][0] = a, G[n][0] = b, G[n][m] = c, G[0][m] = d;
            const auto ab = pointsAlong(e, cache, a, b, n, clash), dc = pointsAlong(e, cache, d, c, n, clash);
            const auto ad = pointsAlong(e, cache, a, d, m, clash), bc = pointsAlong(e, cache, b, c, m, clash);
            for (int i = 1; i < n; ++i) G[i][0] = ab[i - 1], G[i][m] = dc[i - 1];
            for (int j = 1; j < m; ++j) G[0][j] = ad[j - 1], G[n][j] = bc[j - 1];
            const Vec3 A = e.mesh().verts[a], B = e.mesh().verts[b], C = e.mesh().verts[c], D = e.mesh().verts[d];
            for (int i = 1; i < n; ++i)
                for (int j = 1; j < m; ++j) {
                    const double u = (double)i / n, v = (double)j / m;
                    G[i][j] = e.addVert((A * (1 - u) + B * u) * (1 - v) + (D * (1 - u) + C * u) * v);
                }
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < m; ++j) pieces.push_back({G[i][j], G[i + 1][j], G[i + 1][j + 1], G[i][j + 1]});
        } else {
            const int a = f.v[0], b = f.v[1], c = f.v[2];
            const int k = n;
            // P(i, j): a + (b - a) i/k + (c - a) j/k, i + j <= k.
            std::vector<std::vector<int>> P(k + 1, std::vector<int>(k + 1, -1));
            P[0][0] = a, P[k][0] = b, P[0][k] = c;
            const auto ab = pointsAlong(e, cache, a, b, k, clash), bc = pointsAlong(e, cache, b, c, k, clash);
            const auto ac = pointsAlong(e, cache, a, c, k, clash);
            for (int i = 1; i < k; ++i) P[i][0] = ab[i - 1], P[k - i][i] = bc[i - 1], P[0][i] = ac[i - 1];
            const Vec3 A = e.mesh().verts[a], B = e.mesh().verts[b], C = e.mesh().verts[c];
            for (int i = 1; i < k; ++i)
                for (int j = 1; i + j < k; ++j) P[i][j] = e.addVert(A + (B - A) * ((double)i / k) + (C - A) * ((double)j / k));
            for (int i = 0; i < k; ++i)
                for (int j = 0; i + j < k; ++j) {
                    pieces.push_back({P[i][j], P[i + 1][j], P[i][j + 1]});
                    if (i + j + 2 <= k) pieces.push_back({P[i + 1][j], P[i + 1][j + 1], P[i][j + 1]});
                }
        }
        if (clash) {
            if (why) *why = "faces next to each other would split their shared edge differently; use the same number both ways";
            return false;
        }
        std::vector<int> idx;
        if (!replaceWith(e, fi, pieces, &idx)) return false;
        made.insert(idx.begin(), idx.end());
    }
    // Faces round the selection get the points on the edges they share with it.
    if (!insertOnEdges(e, cache, made)) return false;
    if (out) *out = made;
    return true;
}

bool divideEdges(MeshEdit& e, const std::set<Edge>& edges, int n, std::string* why) {
    if (edges.empty() || n < 2) {
        if (why) *why = "divide into at least 2";
        return false;
    }
    std::map<std::pair<int, int>, std::vector<int>> cache;
    bool clash = false;
    for (const Edge& ed : edges) pointsAlong(e, cache, ed.first, ed.second, n, clash);
    if (!insertOnEdges(e, cache, {})) {
        if (why) *why = "a face couldn't take the new points";
        return false;
    }
    return true;
}

bool poke(MeshEdit& e, const std::set<int>& faces, double height, std::set<int>* out) {
    if (faces.empty()) return false;
    std::set<int> made;
    for (int fi : faces) {
        const Face f = e.mesh().faces[fi];
        const Vec3 c = faceCentre(e.mesh(), fi) + faceNormal(e.mesh(), fi) * height;
        const int cv = e.addVert(c);
        std::vector<std::vector<int>> pieces;
        for (int k = 0; k < f.n; ++k) pieces.push_back({f.v[k], f.v[(k + 1) % f.n], cv});
        std::vector<int> idx;
        if (!replaceWith(e, fi, pieces, &idx)) return false;
        made.insert(idx.begin(), idx.end());
    }
    if (out) *out = made;
    return true;
}

bool connectPoints(MeshEdit& e, int a, int b, std::string* why) {
    auto fail = [&](const std::string& w) {
        if (why) *why = w;
        return false;
    };
    if (a == b) return fail("pick two different points");
    const Mesh& m = e.mesh();
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Face& f = m.faces[fi];
        int ia = -1, ib = -1;
        for (int k = 0; k < f.n; ++k) {
            if (f.v[k] == a) ia = k;
            if (f.v[k] == b) ib = k;
        }
        if (ia < 0 || ib < 0) continue;
        if ((ia + 1) % f.n == ib || (ib + 1) % f.n == ia) return fail("those two points are already joined by an edge");
        // Two loops: a ... b and b ... a.
        std::vector<int> l1, l2;
        for (int k = ia;; k = (k + 1) % f.n) {
            l1.push_back(f.v[k]);
            if (k == ib) break;
        }
        for (int k = ib;; k = (k + 1) % f.n) {
            l2.push_back(f.v[k]);
            if (k == ia) break;
        }
        if (!replaceWith(e, fi, {l1, l2})) return fail("the face can't be split there");
        return true;
    }
    return fail("the two points aren't on the same face");
}

int slideTarget(const Mesh& m, int v, const Vec3& dir) {
    if (length(dir) < 1e-9) return -1;
    std::set<int> others;
    for (const Face& f : m.faces)
        for (int k = 0; k < f.n; ++k) {
            if (f.v[k] == v) others.insert(f.v[(k + 1) % f.n]), others.insert(f.v[(k + f.n - 1) % f.n]);
        }
    int best = -1;
    double bestCos = -2;
    for (int o : others) {
        const Vec3 d = m.verts[o] - m.verts[v];
        if (length(d) < 1e-9) continue;
        const double c = dot(d, dir) / (length(d) * length(dir));
        if (c > bestCos) bestCos = c, best = o;
    }
    return best;
}

} // namespace mazz
