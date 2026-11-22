#include "core/bevel_ops.hpp"
#include "core/clean_ops.hpp"
#include "core/draw_ops.hpp"
#include "core/model_ops.hpp"
#include "core/numbers.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <numeric>

namespace mazz {

namespace {

constexpr double kMinArea = 1e-3; // cm²: smaller faces count as gone (as in clean_ops)

bool fail(std::string* why, const char* w) {
    if (why) *why = w;
    return false;
}

Vec3 unit(const Vec3& v) {
    const double l = length(v);
    return l > 0 ? v * (1.0 / l) : Vec3{0, 0, 0};
}

// Area-weighted normal of a polygon (Newell); its length is twice the area.
Vec3 polyNormal(const Mesh& m, const std::vector<int>& p) {
    Vec3 n{0, 0, 0};
    for (size_t i = 0; i < p.size(); ++i) {
        const Vec3 a = m.verts[p[i]], b = m.verts[p[(i + 1) % p.size()]];
        n = n + cross(a, b);
    }
    return n;
}

Face faceLike(const Face& like, const std::vector<int>& v) {
    Face f = like;
    f.n = (int)v.size();
    for (int k = 0; k < f.n; ++k) f.v[k] = v[k];
    f.diag = false;
    return f;
}

// Puts a polygon into face slot `slot` (or a new face when slot < 0), split into 3- and
// 4-corner faces facing `normal` when it has more corners. The faces made go into
// `made`. False if it can't be split or a piece turns over or is too small.
bool placePolygon(MeshEdit& e, int slot, const Face& like, std::vector<int> poly, const Vec3& normal,
                  std::vector<int>* made) {
    // Drop repeated corners next to each other.
    std::vector<int> p;
    for (int v : poly)
        if (p.empty() || p.back() != v) p.push_back(v);
    while (p.size() > 1 && p.front() == p.back()) p.pop_back();
    if (p.size() < 3) return false;
    std::vector<std::vector<int>> pieces;
    if (p.size() <= 4) pieces.push_back(p);
    else pieces = splitPolygon(e.mesh(), p, unit(normal));
    if (pieces.empty()) return false;
    for (const auto& q : pieces) {
        const Vec3 n = polyNormal(e.mesh(), q);
        if (length(n) * 0.5 < kMinArea || dot(n, normal) <= 0) return false;
    }
    for (size_t i = 0; i < pieces.size(); ++i) {
        const Face f = faceLike(like, pieces[i]);
        int idx;
        if (i == 0 && slot >= 0) {
            e.setFace(slot, f);
            idx = slot;
        } else {
            idx = e.addFace(f);
        }
        if (made) made->push_back(idx);
    }
    return true;
}

// The faces round point v in walk order: face i has corners (x[i], v, y[i]) one after
// the other, and x[i + 1] == y[i]. False if they don't go all the way round.
struct Fan {
    std::vector<int> face, corner, x, y;
    int size() const { return (int)face.size(); }
};
bool fanAround(const Mesh& m, const std::vector<std::vector<std::pair<int, int>>>& cornersAt, int v, Fan& fan) {
    std::map<int, std::pair<int, int>> byPrev; // x -> (face, corner)
    for (const auto& [fi, k] : cornersAt[v]) {
        const Face& f = m.faces[fi];
        const int x = f.v[(k + f.n - 1) % f.n];
        if (byPrev.count(x)) return false;
        byPrev[x] = {fi, k};
    }
    if (byPrev.empty()) return false;
    int x = byPrev.begin()->first;
    const int start = x;
    do {
        auto it = byPrev.find(x);
        if (it == byPrev.end() || fan.face.size() >= byPrev.size()) return false;
        const auto [fi, k] = it->second;
        const Face& f = m.faces[fi];
        fan.face.push_back(fi);
        fan.corner.push_back(k);
        fan.x.push_back(x);
        fan.y.push_back(f.v[(k + 1) % f.n]);
        x = fan.y.back();
    } while (x != start);
    return fan.face.size() == byPrev.size();
}

std::vector<std::vector<std::pair<int, int>>> cornersOf(const Mesh& m) {
    std::vector<std::vector<std::pair<int, int>>> at(m.verts.size());
    for (int fi = 0; fi < (int)m.faces.size(); ++fi)
        for (int k = 0; k < m.faces[fi].n; ++k) at[m.faces[fi].v[k]].push_back({fi, k});
    return at;
}

// What a bevel changes: corners of faces replaced by lists of points, and new faces.
struct BevelPlan {
    std::map<std::pair<int, int>, std::vector<int>> repl; // (face, corner) -> points
    struct NewFace {
        std::vector<int> poly;
        int like; // face whose settings it takes
        Vec3 normal;
    };
    std::vector<NewFace> added;
    std::map<Edge, double> slid; // how far new points went along each edge (both ends added up)
};

// Applies the plan: changed faces, then the new ones; points no face uses go. `out`
// gets the new faces.
bool applyPlan(MeshEdit& e, const BevelPlan& plan, std::set<int>* out, std::string* why) {
    const Mesh& m = e.mesh();
    for (const auto& [ed, d] : plan.slid)
        if (d >= length(m.verts[ed.first] - m.verts[ed.second]) - 1e-6)
            return fail(why, "too wide for the faces beside it");
    std::set<int> changed;
    for (const auto& kv : plan.repl) changed.insert(kv.first.first);
    const int nf = (int)m.faces.size();
    std::vector<Vec3> oldNormal(nf);
    for (int fi : changed) oldNormal[fi] = polyNormal(m, std::vector<int>(m.faces[fi].v, m.faces[fi].v + m.faces[fi].n));
    for (int fi : changed) {
        const Face f = e.mesh().faces[fi];
        std::vector<int> poly;
        for (int k = 0; k < f.n; ++k) {
            auto it = plan.repl.find({fi, k});
            if (it == plan.repl.end()) poly.push_back(f.v[k]);
            else poly.insert(poly.end(), it->second.begin(), it->second.end());
        }
        if (!placePolygon(e, fi, f, poly, oldNormal[fi], nullptr)) return fail(why, "too wide for the faces beside it");
    }
    std::vector<int> made;
    // New faces face the way they are built (a bevel ending where the surface goes on
    // straight ends in a small triangle standing across it).
    for (const auto& nfc : plan.added) {
        const Face like = e.mesh().faces[nfc.like];
        const Vec3 own = polyNormal(e.mesh(), nfc.poly);
        if (!placePolygon(e, -1, like, nfc.poly, length(own) > 0 ? own : nfc.normal, &made))
            return fail(why, "too wide for the faces beside it");
    }
    std::vector<int> fmap;
    e.deleteFaces({}, &fmap);
    if (out) {
        out->clear();
        for (int f : made) out->insert(fmap[f]);
    }
    return true;
}

} // namespace

std::set<Edge> innerEdges(const Mesh& m, const std::set<Edge>& edges) {
    std::set<std::pair<int, int>> directed;
    for (const Face& f : m.faces)
        for (int k = 0; k < f.n; ++k) directed.insert({f.v[k], f.v[(k + 1) % f.n]});
    std::set<Edge> out;
    for (const Edge& ed : edges)
        if (directed.count({ed.first, ed.second}) && directed.count({ed.second, ed.first})) out.insert(ed);
    return out;
}

// ---------------------------------------------------------------- bevel edges

bool bevelEdges(MeshEdit& e, const std::set<Edge>& edgesIn, const BevelParams& p, std::set<int>* out, std::string* why) {
    const Mesh& m = e.mesh();
    if (p.width <= 0) return fail(why, "the width must be more than 0");
    const int segs = std::clamp(p.segments, 1, 3);
    const std::set<Edge> edges = innerEdges(m, edgesIn);
    if (edges.empty()) return fail(why, "select edges with a face on both sides");
    const bool closed = isClosed(m);
    const auto at = cornersOf(m);
    std::set<int> touched;
    for (const Edge& ed : edges) touched.insert(ed.first), touched.insert(ed.second);
    // Directed edge -> the face that has it.
    std::map<std::pair<int, int>, int> faceOf;
    for (int fi = 0; fi < (int)m.faces.size(); ++fi)
        for (int k = 0; k < m.faces[fi].n; ++k) faceOf[{m.faces[fi].v[k], m.faces[fi].v[(k + 1) % m.faces[fi].n]}] = fi;

    BevelPlan plan;
    // Rows of points across each bevelled edge's ends: ends[{v, other}] runs from the side
    // of the face with v->other to the side of the face with other->v. `main` are the
    // segments' ends in it; the rest lie on the row between them (a flat end).
    struct Row {
        std::vector<int> pts, main;
    };
    std::map<std::pair<int, int>, Row> ends;
    const double w = p.width;
    for (int v : touched) {
        Fan fan;
        if (!fanAround(m, at, v, fan)) return fail(why, "a bevelled edge ends at an open edge or an odd point");
        const int k = fan.size();
        const Vec3 V = m.verts[v];
        std::vector<int> bev; // faces i whose edge (v, x[i]) is bevelled
        for (int i = 0; i < k; ++i)
            if (edges.count(makeEdge(v, fan.x[i]))) bev.push_back(i);
        const int c = (int)bev.size();
        // Each sector: the faces from one bevelled edge to the next; its points in walk order.
        std::vector<std::vector<int>> sectorPts(c);
        for (int j = 0; j < c; ++j) {
            const int s = bev[j];
            int t = c == 1 ? s + k : bev[(j + 1) % c];
            if (t <= s) t += k;
            const int cnt = t - s;
            auto F = [&](int i) { return i % k; };
            const Vec3 uS = unit(m.verts[fan.x[s]] - V);
            const Vec3 uT = unit(m.verts[fan.y[F(t - 1)]] - V);
            auto& pts = sectorPts[j];
            if (cnt == 1) {
                const double sn = length(cross(uS, uT));
                if (sn < 1e-6) return fail(why, "the bevelled edges lie in a straight line");
                const int ip = e.addVert(V + (uS + uT) * (w / sn));
                plan.repl[{fan.face[s], fan.corner[s]}] = {ip};
                pts = {ip};
                continue;
            }
            const int g1 = fan.y[s], gL = fan.x[F(t - 1)];
            const Vec3 u1 = unit(m.verts[g1] - V), uL = unit(m.verts[gL] - V);
            const double s1 = length(cross(uS, u1)), sL = length(cross(uT, uL));
            if (s1 < 1e-6 || sL < 1e-6) return fail(why, "an edge beside the bevel lies in line with it");
            const double d1 = w / s1, dL = w / sL;
            if (cnt == 2) {
                // One edge between two bevelled ones: one point on it, at their average.
                const double d = (d1 + dL) / 2;
                const int ip = e.addVert(V + u1 * d);
                plan.slid[makeEdge(v, g1)] += d;
                plan.repl[{fan.face[s], fan.corner[s]}] = {ip};
                plan.repl[{fan.face[F(s + 1)], fan.corner[F(s + 1)]}] = {ip};
                pts = {ip};
                continue;
            }
            const int Ps = e.addVert(V + u1 * d1), Pt = e.addVert(V + uL * dL);
            plan.slid[makeEdge(v, g1)] += d1;
            plan.slid[makeEdge(v, gL)] += dL;
            plan.repl[{fan.face[s], fan.corner[s]}] = {Ps};
            plan.repl[{fan.face[F(t - 1)], fan.corner[F(t - 1)]}] = {Pt};
            if (cnt == 3) {
                plan.repl[{fan.face[F(s + 1)], fan.corner[F(s + 1)]}] = {Ps, Pt};
                pts = {Ps, Pt};
            } else {
                plan.repl[{fan.face[F(s + 1)], fan.corner[F(s + 1)]}] = {Ps, v};
                plan.repl[{fan.face[F(t - 2)], fan.corner[F(t - 2)]}] = {v, Pt};
                pts = {Ps, v, Pt};
            }
        }
        // Across each bevelled edge: from the last point of the sector before it to the
        // first of the sector after it, round through the arc's points. An arc point
        // where a point already is (v itself, or another edge's arc on a flat surface)
        // is that point.
        std::vector<int> ring;
        std::vector<int> mine = {v};
        for (const auto& sp : sectorPts) mine.insert(mine.end(), sp.begin(), sp.end());
        auto pointAt = [&](const Vec3& raw) {
            const Vec3 q = clean(raw); // points are stored rounded
            for (int ip : mine)
                if (length(e.mesh().verts[ip] - q) < 1e-6) return ip;
            const int ip = e.addVert(q);
            mine.push_back(ip);
            return ip;
        };
        std::vector<std::pair<int, int>> keys;
        for (int j = 0; j < c; ++j) {
            ring.insert(ring.end(), sectorPts[j].begin(), sectorPts[j].end());
            const int other = fan.x[bev[(j + 1) % c]];
            const int aL = sectorPts[j].back(), aR = sectorPts[(j + 1) % c].front();
            Row row;
            row.pts = {aL};
            const Vec3 AL = e.mesh().verts[aL], AR = e.mesh().verts[aR];
            const Vec3 C = AL + AR - V; // arc centre: tangent to both faces at its ends
            for (int i = 1; i < segs; ++i) {
                double cs, sn;
                cosSinDeg(90.0 * i / segs, cs, sn);
                const int ip = pointAt(C + (AL - C) * cs + (AR - C) * sn);
                row.pts.push_back(ip);
                ring.push_back(ip);
            }
            row.pts.push_back(aR);
            for (int i = 0; i <= segs; ++i) row.main.push_back(i);
            ends[{v, other}] = row;
            keys.push_back({v, other});
        }
        // One bevelled edge whose end cap is a single face: the arc goes into that face.
        if (c == 1 && segs > 1 && k == 3) {
            const int mid = (bev[0] + 1) % k;
            std::vector<int> row = ends[keys[0]].pts; // Pt .. Ps
            std::reverse(row.begin(), row.end());   // Ps .. Pt
            plan.repl[{fan.face[mid], fan.corner[mid]}] = row;
            continue;
        }
        // The patch where the ends meet (walk order goes clockwise seen from outside).
        std::vector<int> patch;
        for (int ip : ring)
            if (patch.empty() || patch.back() != ip) patch.push_back(ip);
        while (patch.size() > 1 && patch.front() == patch.back()) patch.pop_back();
        if (patch.size() < 3) continue;
        if (length(polyNormal(e.mesh(), patch)) * 0.5 >= kMinArea) {
            std::reverse(patch.begin(), patch.end());
            Vec3 n{0, 0, 0};
            for (int i = 0; i < k; ++i) n = n + faceNormal(m, fan.face[i]);
            plan.added.push_back({patch, fan.face[0], n});
            continue;
        }
        // A flat end: no patch. One bevelled edge ending where the surface goes on flat:
        // v lies on its end row, between the arc's points.
        if (c == 1) {
            Row& row = ends[keys[0]];
            if (std::find(row.pts.begin(), row.pts.end(), v) == row.pts.end()) {
                const Vec3 A = e.mesh().verts[row.pts.front()], d = e.mesh().verts[row.pts.back()] - A;
                const double tv = dot(V - A, d);
                size_t at = 1;
                while (at < row.pts.size() && dot(e.mesh().verts[row.pts[at]] - A, d) < tv) ++at;
                row.pts.insert(row.pts.begin() + at, v);
                for (int& mi : row.main)
                    if (mi >= (int)at) ++mi;
            }
            // The faces beside the row (the sector's second and second-last, which
            // keep v) get the row's points on their side of v too.
            const int iv = (int)(std::find(row.pts.begin(), row.pts.end(), v) - row.pts.begin());
            const int s0 = bev[0];
            const int fA = (s0 + 1) % k, fB = (s0 + k - 2) % k; // {Ps, v} and {v, Pt}
            std::vector<int> a = {row.pts.back()};             // Ps .. v
            for (int t = (int)row.pts.size() - 2; t > iv; --t) a.push_back(row.pts[t]);
            a.push_back(v);
            std::vector<int> b = {v};                          // v .. Pt
            for (int t = iv - 1; t > 0; --t) b.push_back(row.pts[t]);
            b.push_back(row.pts.front());
            plan.repl[{fan.face[fA], fan.corner[fA]}] = a;
            plan.repl[{fan.face[fB], fan.corner[fB]}] = b;
        }
    }
    // The strips along the edges.
    for (const Edge& ed : edges) {
        const int a = ed.first, b = ed.second;
        const int f1 = faceOf.at({a, b}), f2 = faceOf.at({b, a});
        const Row& ra = ends.at({a, b}); // f1 side -> f2 side
        Row rb = ends.at({b, a});        // f2 side -> f1 side: turned round
        std::reverse(rb.pts.begin(), rb.pts.end());
        std::reverse(rb.main.begin(), rb.main.end());
        for (int& mi : rb.main) mi = (int)rb.pts.size() - 1 - mi;
        const Vec3 n = faceNormal(m, f1) + faceNormal(m, f2);
        for (int i = 0; i < segs; ++i) {
            std::vector<int> poly;
            for (int t = ra.main[i]; t <= ra.main[i + 1]; ++t) poly.push_back(ra.pts[t]);
            for (int t = rb.main[i + 1]; t >= rb.main[i]; --t) poly.push_back(rb.pts[t]);
            plan.added.push_back({poly, f1, n});
        }
    }
    if (!applyPlan(e, plan, out, why)) return false;
    if (closed && !isClosed(e.mesh())) return fail(why, "it would open the model");
    return true;
}

// ---------------------------------------------------------------- bevel points

bool bevelPoints(MeshEdit& e, const std::set<int>& verts, double width, std::set<int>* out, std::string* why) {
    const Mesh& m = e.mesh();
    if (width <= 0) return fail(why, "the width must be more than 0");
    if (verts.empty()) return fail(why, "select points");
    const bool closed = isClosed(m);
    const auto at = cornersOf(m);
    BevelPlan plan;
    for (int v : verts) {
        Fan fan;
        if (!fanAround(m, at, v, fan)) return fail(why, "a point is on an open edge or an odd point");
        const int k = fan.size();
        const Vec3 V = m.verts[v];
        std::vector<int> onEdge(k); // on edge (v, x[i])
        for (int i = 0; i < k; ++i) {
            onEdge[i] = e.addVert(V + unit(m.verts[fan.x[i]] - V) * width);
            plan.slid[makeEdge(v, fan.x[i])] += width;
        }
        for (int i = 0; i < k; ++i) plan.repl[{fan.face[i], fan.corner[i]}] = {onEdge[i], onEdge[(i + 1) % k]};
        std::vector<int> patch(onEdge.rbegin(), onEdge.rend());
        Vec3 n{0, 0, 0};
        for (int i = 0; i < k; ++i) n = n + faceNormal(m, fan.face[i]);
        plan.added.push_back({patch, fan.face[0], n});
    }
    if (!applyPlan(e, plan, out, why)) return false;
    if (closed && !isClosed(e.mesh())) return fail(why, "it would open the model");
    return true;
}

// ---------------------------------------------------------------- bridge

namespace {

// Joins two outlines by a tube. Each runs as the outline of a removed face would (seen
// from outside); the tube goes along A forwards and along B backwards.
bool bridgeLoops(MeshEdit& e, std::vector<int> A, std::vector<int> B, const BridgeParams& p, int like,
                 std::set<int>* out, std::string* why) {
    if (A.size() < 3 || B.size() < 3) return fail(why, "an outline has fewer than 3 points");
    if (A.size() < B.size()) std::swap(A, B);
    const int n1 = (int)A.size(), n2 = (int)B.size();
    const int segs = std::clamp(p.segments, 1, 32);
    auto P = [&](int v) { return e.mesh().verts[v]; };
    auto jOf = [&](int i) { return (int)std::floor((double)i * n2 / n1 + 0.5); };
    auto mod = [](int a, int n) { return ((a % n) + n) % n; };
    // Where along B (backwards from `off`) A's point i lands: a fraction of B's points.
    auto bPos = [&](int off, double j) {
        const double t = std::floor(j);
        const int b0 = mod(off - (int)t, n2), b1 = mod(off - (int)t - 1, n2);
        return P(B[b0]) + (P(B[b1]) - P(B[b0])) * (j - t);
    };
    int best = 0;
    double bestCost = 1e300;
    for (int off = 0; off < n2; ++off) {
        double cost = 0;
        for (int i = 0; i < n1; ++i) {
            const Vec3 d = P(A[i]) - bPos(off, (double)i * n2 / n1);
            cost += dot(d, d);
        }
        if (cost < bestCost - 1e-9) bestCost = cost, best = off;
    }
    const int off = mod(best + p.twist, n2);
    // Rings between: A's count of points, part way to B.
    std::vector<std::vector<int>> rings = {A};
    for (int r = 1; r < segs; ++r) {
        std::vector<int> ring;
        for (int i = 0; i < n1; ++i) {
            const Vec3 q = P(A[i]) + (bPos(off, (double)i * n2 / n1) - P(A[i])) * ((double)r / segs);
            ring.push_back(e.addVert(q));
        }
        rings.push_back(ring);
    }
    std::vector<std::vector<int>> polys;
    for (int r = 0; r + 1 < segs; ++r)
        for (int i = 0; i < n1; ++i) {
            const int i1 = (i + 1) % n1;
            polys.push_back({rings[r][i], rings[r][i1], rings[r + 1][i1], rings[r + 1][i]});
        }
    const auto& R = rings.back();
    for (int i = 0; i < n1; ++i) {
        const int i1 = (i + 1) % n1;
        const int j0 = jOf(i), j1 = jOf(i + 1);
        const int b0 = B[mod(off - j0, n2)], b1 = B[mod(off - j1, n2)];
        if (j0 == j1) polys.push_back({R[i], R[i1], b0});
        else polys.push_back({R[i], R[i1], b1, b0});
    }
    const Face likeF = e.mesh().faces.empty() ? Face{} : e.mesh().faces[std::min(like, (int)e.mesh().faces.size() - 1)];
    if (out) out->clear();
    for (const auto& q : polys) {
        const Vec3 n = polyNormal(e.mesh(), q);
        if (length(n) * 0.5 < kMinArea) return fail(why, "the tube would have a face with no area");
        Face f = faceLike(likeF, q);
        f.twoSided = false;
        const int idx = e.addFace(f);
        if (out) out->insert(idx);
    }
    return true;
}

} // namespace

bool bridgeFaces(MeshEdit& e, const std::set<int>& faces, const BridgeParams& p, std::set<int>* out, std::string* why) {
    const Mesh& m = e.mesh();
    if (faces.size() < 2) return fail(why, "select two faces, or two blocks of faces");
    const bool closed = isClosed(m);
    // Blocks: faces joined by an edge.
    std::map<std::pair<int, int>, int> owner;
    for (int fi : faces)
        for (int k = 0; k < m.faces[fi].n; ++k) owner[{m.faces[fi].v[k], m.faces[fi].v[(k + 1) % m.faces[fi].n]}] = fi;
    std::map<int, int> parent;
    for (int fi : faces) parent[fi] = fi;
    std::function<int(int)> find = [&](int x) { return parent[x] == x ? x : parent[x] = find(parent[x]); };
    for (const auto& [d, fi] : owner) {
        auto it = owner.find({d.second, d.first});
        if (it != owner.end()) parent[find(fi)] = find(it->second);
    }
    std::map<int, std::set<int>> blocks;
    for (int fi : faces) blocks[find(fi)].insert(fi);
    if (blocks.size() != 2) return fail(why, "select two faces, or two blocks of faces (not touching each other)");
    std::vector<std::vector<int>> loops;
    for (const auto& [r, block] : blocks) {
        (void)r;
        loops.push_back(regionOutline(m, block));
        if (loops.back().empty()) return fail(why, "a block of faces has a hole in it");
    }
    for (int v : loops[0])
        if (std::find(loops[1].begin(), loops[1].end(), v) != loops[1].end())
            return fail(why, "the two blocks touch; pick ones apart from each other");
    const int like = *faces.begin();
    std::vector<int> fmap, vmap;
    e.deleteFaces(faces, &fmap, &vmap);
    for (auto& l : loops)
        for (int& v : l) v = vmap[v];
    int likeNew = 0;
    for (int fi = 0; fi < (int)fmap.size(); ++fi)
        if (fmap[fi] >= 0) {
            likeNew = fmap[fi];
            break;
        }
    (void)like;
    if (!bridgeLoops(e, loops[0], loops[1], p, likeNew, out, why)) return false;
    if (closed && !isClosed(e.mesh())) return fail(why, "it would open the model");
    return true;
}

bool bridgeHoles(MeshEdit& e, const std::set<Edge>& edges, const BridgeParams& p, std::set<int>* out, std::string* why) {
    const Mesh& m = e.mesh();
    std::set<std::pair<int, int>> directed;
    int like = 0;
    for (int fi = 0; fi < (int)m.faces.size(); ++fi)
        for (int k = 0; k < m.faces[fi].n; ++k) directed.insert({m.faces[fi].v[k], m.faces[fi].v[(k + 1) % m.faces[fi].n]});
    // A hole's outline as a removed face would have it: each open edge x->y gives y->x.
    std::map<int, int> next;
    for (const auto& d : directed)
        if (!directed.count({d.second, d.first})) {
            if (next.count(d.second)) return fail(why, "a hole's edge meets another one at a point");
            next[d.second] = d.first;
        }
    std::vector<std::vector<int>> loops;
    std::set<int> used;
    for (const Edge& ed : edges)
        for (int start : {ed.first, ed.second}) {
            if (!next.count(start) || used.count(start)) continue;
            std::vector<int> loop;
            int v = start;
            do {
                loop.push_back(v);
                used.insert(v);
                auto it = next.find(v);
                if (it == next.end() || loop.size() > next.size()) return fail(why, "a hole's outline doesn't close");
                v = it->second;
            } while (v != start);
            loops.push_back(loop);
        }
    if (loops.size() != 2) return fail(why, "select an edge on each of two holes");
    if (!bridgeLoops(e, loops[0], loops[1], p, like, out, why)) return false;
    return true;
}

// ---------------------------------------------------------------- shell

bool shell(MeshEdit& e, const std::set<int>& seed, const ShellParams& p, std::set<int>* out, std::string* why) {
    const Mesh& m = e.mesh();
    if (p.thickness <= 0) return fail(why, "the thickness must be more than 0");
    if (seed.empty()) return fail(why, "select the surface to shell");
    std::vector<int> pieceOf;
    pieceIds(m, pieceOf);
    std::set<int> pieces;
    for (int fi : seed) pieces.insert(pieceOf[fi]);
    std::vector<int> region;
    for (int fi = 0; fi < (int)m.faces.size(); ++fi)
        if (pieces.count(pieceOf[fi])) region.push_back(fi);
    std::set<std::pair<int, int>> directed;
    for (int fi : region)
        for (int k = 0; k < m.faces[fi].n; ++k) directed.insert({m.faces[fi].v[k], m.faces[fi].v[(k + 1) % m.faces[fi].n]});
    std::vector<std::pair<int, int>> open;
    for (const auto& d : directed)
        if (!directed.count({d.second, d.first})) open.push_back(d);
    if (open.empty()) return fail(why, "it is closed already: there is nothing to shell");
    // Point normals: the faces' normals weighted by their angle at the point.
    const int nv = (int)m.verts.size();
    std::vector<Vec3> pn(nv, Vec3{0, 0, 0});
    std::vector<std::vector<Vec3>> fnAt(nv);
    for (int fi : region) {
        const Face& f = m.faces[fi];
        const Vec3 fn = faceNormal(m, fi);
        if (length(fn) == 0) return fail(why, "a face has no area");
        for (int k = 0; k < f.n; ++k) {
            const Vec3 V = m.verts[f.v[k]];
            const Vec3 a = unit(m.verts[f.v[(k + f.n - 1) % f.n]] - V), b = unit(m.verts[f.v[(k + 1) % f.n]] - V);
            const double ang = std::acos(std::clamp(dot(a, b), -1.0, 1.0));
            pn[f.v[k]] = pn[f.v[k]] + fn * ang;
            fnAt[f.v[k]].push_back(fn);
        }
    }
    const double top = p.side == ShellSide::Outward ? p.thickness : p.side == ShellSide::Centred ? p.thickness / 2 : 0;
    const double bot = p.thickness - top;
    std::map<int, int> below;
    std::vector<Vec3> orig = m.verts;
    for (int v = 0; v < nv; ++v) {
        if (fnAt[v].empty()) continue;
        const Vec3 n = unit(pn[v]);
        if (length(n) == 0) return fail(why, "the surface folds back on itself at a point");
        double mn = 1;
        for (const Vec3& fn : fnAt[v]) mn = std::min(mn, dot(n, fn));
        if (mn < 0.3) return fail(why, "the surface bends too sharply to keep the thickness even");
        const double s = 1.0 / mn; // even thickness across the faces round the point
        below[v] = e.addVert(orig[v] - n * (bot * s));
        if (top > 0) e.setVert(v, orig[v] + n * (top * s));
    }
    std::set<int> all(region.begin(), region.end());
    for (int fi : region) {
        Face f = e.mesh().faces[fi];
        f.twoSided = false;
        e.setFace(fi, f);
        Face g = f;
        for (int k = 0; k < f.n; ++k) g.v[k] = below.at(f.v[f.n - 1 - k]);
        // Turned round, the old v0-v2 diagonal is the new v1-v3: same split, other flag.
        g.diag = f.n == 4 ? !f.diag : false;
        all.insert(e.addFace(g));
    }
    for (const auto& [x, y] : open) {
        Face f = e.mesh().faces[region.front()];
        f.twoSided = false;
        f.diag = false;
        f.n = 4;
        f.v[0] = y, f.v[1] = x, f.v[2] = below.at(x), f.v[3] = below.at(y);
        all.insert(e.addFace(f));
    }
    // The shelled pieces must be closed and face outward, the inner faces not turned over.
    std::set<std::pair<int, int>> d2;
    for (int fi : all)
        for (int k = 0; k < e.mesh().faces[fi].n; ++k)
            d2.insert({e.mesh().faces[fi].v[k], e.mesh().faces[fi].v[(k + 1) % e.mesh().faces[fi].n]});
    for (const auto& d : d2)
        if (!d2.count({d.second, d.first})) return fail(why, "the shell doesn't close");
    for (int fi : all) {
        const Face& f = e.mesh().faces[fi];
        if (length(polyNormal(e.mesh(), std::vector<int>(f.v, f.v + f.n))) * 0.5 < kMinArea)
            return fail(why, "too thick for the surface");
    }
    const int n0 = (int)m.faces.size() - (int)region.size() - (int)open.size();
    for (int i = 0; i < (int)region.size(); ++i) {
        const int fi = region[i], gi = n0 + i;
        if (dot(faceNormal(e.mesh(), gi), faceNormal(e.mesh(), fi)) >= 0) return fail(why, "too thick for the surface");
    }
    if (out) *out = all;
    return true;
}

} // namespace mazz
