#include "core/draw_ops.hpp"
#include "core/numbers.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace mazz {

namespace {

constexpr double kOn = 2e-3;        // cm: a point this near an edge lies on it (clean values are 0.001 apart)
constexpr double kStraight = 1e-5;  // sine of an angle this small: a straight corner
constexpr double kMinArea = 1e-6;   // cm²

struct P2 {
    double x = 0, y = 0;
};
P2 operator-(P2 a, P2 b) { return {a.x - b.x, a.y - b.y}; }
double cross(P2 a, P2 b) { return a.x * b.y - a.y * b.x; }
double dot2(P2 a, P2 b) { return a.x * b.x + a.y * b.y; }
double len(P2 a) { return std::hypot(a.x, a.y); }

// Turn at b going a -> b -> c: > 0 left (convex for a counter-clockwise outline), 0 straight.
double turn(P2 a, P2 b, P2 c);
// Straight when b lies within kOn of the line a-c (points are rounded to 0.001 cm, so a
// point put on an edge can be that far off it), or the angle is tiny.
double turn(P2 a, P2 b, P2 c) {
    const P2 e1 = b - a, e2 = c - b;
    const double l = len(e1) * len(e2);
    if (l <= 0) return 0;
    const double s = cross(e1, e2) / l;
    if (std::fabs(s) < kStraight) return 0;
    const double ac = len(c - a);
    if (ac > 0 && std::fabs(cross(c - a, b - a)) / ac < kOn) return 0;
    return s;
}

struct Frame {
    Vec3 o, u, v, n;
    P2 to(const Vec3& p) const { return {dot(p - o, u), dot(p - o, v)}; }
    Vec3 from(P2 q) const { return o + u * q.x + v * q.y; }
};

Frame frameFor(const Vec3& origin, const Vec3& normal) {
    Frame f;
    f.o = origin;
    f.n = normal;
    const Vec3 a = std::fabs(normal.z) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0};
    Vec3 u = cross(a, normal);
    u = u * (1.0 / length(u));
    f.u = u;
    f.v = cross(normal, u);
    return f;
}

double segPointDist(P2 p, P2 a, P2 b) {
    const P2 d = b - a;
    const double l2 = dot2(d, d);
    double t = l2 > 0 ? dot2(p - a, d) / l2 : 0;
    t = std::clamp(t, 0.0, 1.0);
    return len(p - P2{a.x + d.x * t, a.y + d.y * t});
}

// Proper crossing of segments ab and cd (touching at an end doesn't count when `ends`
// is false).
bool segmentsCross(P2 a, P2 b, P2 c, P2 d) {
    const double d1 = cross(b - a, c - a), d2 = cross(b - a, d - a);
    const double d3 = cross(d - c, a - c), d4 = cross(d - c, b - c);
    const double ea = 1e-9 * (len(b - a) + 1) * (len(d - c) + 1);
    return ((d1 > ea && d2 < -ea) || (d1 < -ea && d2 > ea)) && ((d3 > ea && d4 < -ea) || (d3 < -ea && d4 > ea));
}

bool inTriangle(P2 p, P2 a, P2 b, P2 c) {
    // Inside or on the border (a point on the cut would leave a gap).
    const double s = 1e-9;
    const double d1 = cross(b - a, p - a), d2 = cross(c - b, p - b), d3 = cross(a - c, p - c);
    return d1 >= -s && d2 >= -s && d3 >= -s;
}

double area2(const std::vector<P2>& pts) {
    double a = 0;
    for (size_t i = 0; i < pts.size(); ++i) a += cross(pts[i], pts[(i + 1) % pts.size()]);
    return a / 2;
}

bool pointInPolygon(P2 p, const std::vector<P2>& poly) {
    bool in = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const P2 a = poly[i], b = poly[j];
        if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) in = !in;
    }
    return in;
}

// Ear clipping, then pairs of triangles joined into convex quads.
std::vector<std::vector<int>> splitPoly2D(const std::vector<int>& ids, const std::vector<P2>& pts) {
    const int n = (int)ids.size();
    std::vector<std::vector<int>> out;
    if (n < 3) return out;
    bool convex = true;
    for (int i = 0; i < n && convex; ++i)
        if (turn(pts[(i + n - 1) % n], pts[i], pts[(i + 1) % n]) <= 0) convex = false;
    if (convex) {
        // Strips across the shape (a circle becomes bands, not a fan of slivers).
        int lo = 0, hi = n - 1;
        while (hi - lo + 1 > 4) {
            out.push_back({ids[lo], ids[lo + 1], ids[hi - 1], ids[hi]});
            ++lo;
            --hi;
        }
        std::vector<int> last;
        for (int i = lo; i <= hi; ++i) last.push_back(ids[i]);
        out.push_back(last);
        return out;
    }
    // Ear clipping on positions in the list.
    std::vector<int> L(n);
    for (int i = 0; i < n; ++i) L[i] = i;
    std::vector<std::array<int, 3>> tris;
    int guard = 0;
    // The smallest angle of a triangle (as its sine): ears are cut best-shaped first, so
    // the pieces are as even as possible (no fans of slivers).
    auto quality = [&](int a, int b, int c) {
        const P2 A = pts[a], B = pts[b], C = pts[c];
        const double ab = len(B - A), bc = len(C - B), ca = len(A - C);
        const double area = std::fabs(cross(B - A, C - A));
        if (ab * bc * ca <= 0) return 0.0;
        return std::min({area / (ab * ca), area / (ab * bc), area / (bc * ca)});
    };
    while (L.size() > 3 && guard++ < 10000) {
        const int m = (int)L.size();
        int bestK = -1;
        double bestQ = -1;
        for (int k = 0; k < m; ++k) {
            const int a = L[(k + m - 1) % m], b = L[k], c = L[(k + 1) % m];
            if (turn(pts[a], pts[b], pts[c]) <= 0) continue;
            bool blocked = false;
            for (int q : L) {
                if (q == a || q == b || q == c) continue;
                // The same spot as a corner (a hole's bridge visits points twice): not in the way.
                if (len(pts[q] - pts[a]) < 1e-9 || len(pts[q] - pts[b]) < 1e-9 || len(pts[q] - pts[c]) < 1e-9) continue;
                if (inTriangle(pts[q], pts[a], pts[b], pts[c]) || segPointDist(pts[q], pts[a], pts[c]) < kOn) {
                    blocked = true;
                    break;
                }
            }
            if (blocked) continue;
            const double qv = quality(a, b, c);
            if (qv > bestQ) bestQ = qv, bestK = k;
        }
        if (bestK < 0) return {};
        tris.push_back({L[(bestK + m - 1) % m], L[bestK], L[(bestK + 1) % m]});
        L.erase(L.begin() + bestK);
    }
    if (L.size() == 3) {
        const double t = turn(pts[L[0]], pts[L[1]], pts[L[2]]);
        if (t > 0) tris.push_back({L[0], L[1], L[2]});
        else if (t < 0) return {}; // the rest turned over (a sliver bent the wrong way): can't
        // straight: three points in a line, nothing left to fill
    }
    // Join triangles sharing an edge into convex quads, greedily.
    std::vector<char> used(tris.size(), 0);
    for (size_t t = 0; t < tris.size(); ++t) {
        if (used[t]) continue;
        int best = -1;
        std::array<int, 4> bestQuad{};
        double bestScore = 0;
        for (size_t s = 0; s < tris.size(); ++s) {
            if (s == t || used[s]) continue;
            for (int i = 0; i < 3; ++i) {
                const int x = tris[t][i], y = tris[t][(i + 1) % 3], p = tris[t][(i + 2) % 3];
                for (int j = 0; j < 3; ++j) {
                    if (tris[s][j] != y || tris[s][(j + 1) % 3] != x) continue;
                    const int q = tris[s][(j + 2) % 3];
                    const std::array<int, 4> quad{x, q, y, p};
                    double worst = 1;
                    for (int k = 0; k < 4; ++k) worst = std::min(worst, turn(pts[quad[(k + 3) % 4]], pts[quad[k]], pts[quad[(k + 1) % 4]]));
                    if (worst > 0 && worst > bestScore) {
                        bestScore = worst;
                        best = (int)s;
                        bestQuad = quad;
                    }
                }
            }
        }
        used[t] = 1;
        if (best >= 0) {
            used[best] = 1;
            out.push_back({ids[bestQuad[0]], ids[bestQuad[1]], ids[bestQuad[2]], ids[bestQuad[3]]});
        } else {
            out.push_back({ids[tris[t][0]], ids[tris[t][1]], ids[tris[t][2]]});
        }
    }
    return out;
}

constexpr double kMinPiece = 1e-3; // cm²: a cut leaving a smaller face is refused

double pieceArea(const Mesh& m, const std::vector<int>& p) {
    Vec3 s{0, 0, 0};
    for (size_t k = 1; k + 1 < p.size(); ++k) s = s + cross(m.verts[p[k]] - m.verts[p[0]], m.verts[p[k + 1]] - m.verts[p[0]]);
    return length(s) / 2;
}

bool piecesOk(const Mesh& m, const std::vector<std::vector<int>>& pieces) {
    if (pieces.empty()) return false;
    for (const auto& p : pieces)
        if (pieceArea(m, p) < kMinPiece) return false;
    return true;
}

Face faceLike(const Face& like, const std::vector<int>& vs) {
    Face f;
    f.tex = like.tex;
    f.group = like.group;
    f.sg = like.sg;
    f.twoSided = like.twoSided;
    f.n = (int)vs.size();
    for (int k = 0; k < f.n; ++k) f.v[k] = vs[k];
    return f;
}

// Replaces face `fi` by the given pieces (the first takes its place). Returns their indices.
std::vector<int> replaceFace(MeshEdit& e, int fi, const std::vector<std::vector<int>>& pieces) {
    const Face like = e.mesh().faces[fi];
    std::vector<int> out;
    for (size_t i = 0; i < pieces.size(); ++i) {
        const Face f = faceLike(like, pieces[i]);
        if (i == 0) {
            e.setFace(fi, f);
            out.push_back(fi);
        } else {
            out.push_back(e.addFace(f));
        }
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------- frames and places

bool faceFrame(const Mesh& m, int face, Vec3& origin, Vec3& u, Vec3& v, Vec3& n) {
    if (face < 0 || face >= (int)m.faces.size()) return false;
    const Face& f = m.faces[face];
    n = faceNormal(m, face);
    if (length(n) < 0.5) return false;
    origin = m.verts[f.v[0]];
    Vec3 e = m.verts[f.v[1]] - origin;
    e = e - n * dot(e, n);
    if (length(e) < 1e-9) return false;
    u = e * (1.0 / length(e));
    v = cross(n, u);
    return true;
}

std::vector<std::vector<int>> splitPolygon(const Mesh& m, const std::vector<int>& poly, const Vec3& normal) {
    if (poly.size() < 3) return {};
    const Frame fr = frameFor(m.verts[poly[0]], normal);
    std::vector<P2> pts;
    for (int i : poly) pts.push_back(fr.to(m.verts[i]));
    if (area2(pts) <= 0) return {};
    std::vector<int> local(poly.size());
    for (size_t i = 0; i < poly.size(); ++i) local[i] = (int)i;
    auto pieces = splitPoly2D(local, pts);
    for (auto& p : pieces)
        for (int& i : p) i = poly[i];
    return pieces;
}

FacePlace placeOnFace(const Mesh& m, int face, const Vec3& p, int* edgeIndex, int* corner) {
    Vec3 o, u, v, n;
    if (!faceFrame(m, face, o, u, v, n)) return FacePlace::Outside;
    if (std::fabs(dot(p - o, n)) > kOn) return FacePlace::Outside;
    const Frame fr{o, u, v, n};
    const Face& f = m.faces[face];
    std::vector<P2> poly;
    for (int k = 0; k < f.n; ++k) poly.push_back(fr.to(m.verts[f.v[k]]));
    const P2 q = fr.to(p);
    for (int k = 0; k < f.n; ++k)
        if (len(q - poly[k]) <= kOn) {
            if (corner) *corner = k;
            return FacePlace::OnCorner;
        }
    for (int k = 0; k < f.n; ++k)
        if (segPointDist(q, poly[k], poly[(k + 1) % f.n]) <= kOn) {
            if (edgeIndex) *edgeIndex = k;
            return FacePlace::OnEdge;
        }
    return pointInPolygon(q, poly) ? FacePlace::Inside : FacePlace::Outside;
}

std::vector<Vec3> rectanglePoints(const Vec3& c, const Vec3& u, const Vec3& v, double w, double h) {
    std::vector<Vec3> p = {c, c + u * w, c + u * w + v * h, c + v * h};
    if ((w < 0) != (h < 0)) std::reverse(p.begin(), p.end()); // keep it counter-clockwise
    return p;
}

std::vector<Vec3> circlePoints(const Vec3& centre, const Vec3& u, const Vec3& v, double r, int segments) {
    std::vector<Vec3> p;
    segments = std::max(3, segments);
    for (int i = 0; i < segments; ++i) {
        double c, s;
        cosSinDeg(360.0 * i / segments, c, s);
        p.push_back(centre + u * (r * c) + v * (r * s));
    }
    return p;
}

// ---------------------------------------------------------------- surfaces

namespace {

// The outline loops of a set of faces: each a closed chain of its border edges, in the
// faces' direction. False if the border touches itself at a point.
bool outlineLoops(const Mesh& m, const std::set<int>& region, std::vector<std::vector<int>>& loops) {
    loops.clear();
    std::set<std::pair<int, int>> all;
    for (int fi : region) {
        const Face& f = m.faces[fi];
        for (int k = 0; k < f.n; ++k) all.insert({f.v[k], f.v[(k + 1) % f.n]});
    }
    std::map<int, int> next;
    for (const auto& d : all)
        if (!all.count({d.second, d.first})) {
            if (next.count(d.first)) return false;
            next[d.first] = d.second;
        }
    std::set<int> seen;
    for (const auto& [start, nx] : next) {
        (void)nx;
        if (seen.count(start)) continue;
        std::vector<int> loop;
        int v = start;
        do {
            if (seen.count(v)) return false;
            seen.insert(v);
            loop.push_back(v);
            auto it = next.find(v);
            if (it == next.end()) return false;
            v = it->second;
        } while (v != start);
        loops.push_back(loop);
    }
    return !loops.empty();
}

// The region's outline as one loop (empty if it has holes or isn't one piece).
std::vector<int> outlineLoop(const Mesh& m, const std::set<int>& region) {
    std::vector<std::vector<int>> loops;
    if (!outlineLoops(m, region, loops) || loops.size() != 1) return {};
    return loops[0];
}

// A flat surface: the faces lying flat with a face and joined to it (to the user it is
// one face, however it has been cut), its outline and its holes.
struct Surface {
    std::set<int> faces;
    Frame fr;
    std::vector<int> outer;              // counter-clockwise
    std::vector<std::vector<int>> holes; // clockwise
};

bool surfaceOf(const Mesh& m, int face, Surface& s) {
    Vec3 o, u, v, n;
    if (!faceFrame(m, face, o, u, v, n)) return false;
    s = Surface{};
    s.fr = Frame{o, u, v, n};
    s.faces = {face};
    std::vector<int> todo = {face};
    std::map<std::pair<int, int>, int> owner; // directed edge -> face
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Face& f = m.faces[fi];
        for (int k = 0; k < f.n; ++k) owner[{f.v[k], f.v[(k + 1) % f.n]}] = fi;
    }
    while (!todo.empty()) {
        const Face f = m.faces[todo.back()];
        todo.pop_back();
        for (int k = 0; k < f.n; ++k) {
            auto it = owner.find({f.v[(k + 1) % f.n], f.v[k]});
            if (it == owner.end() || s.faces.count(it->second)) continue;
            const int g = it->second;
            if (dot(faceNormal(m, g), n) < 1 - 1e-9 || std::fabs(dot(faceCentre(m, g) - o, n)) > 1e-6) continue;
            s.faces.insert(g);
            todo.push_back(g);
        }
    }
    std::vector<std::vector<int>> loops;
    if (!outlineLoops(m, s.faces, loops)) return false;
    for (auto& l : loops) {
        std::vector<P2> p;
        for (int i : l) p.push_back(s.fr.to(m.verts[i]));
        if (area2(p) > 0) {
            if (!s.outer.empty()) return false; // two outlines: not one surface
            s.outer = l;
        } else {
            s.holes.push_back(l);
        }
    }
    return !s.outer.empty();
}

std::vector<P2> positions(const Mesh& m, const Frame& fr, const std::vector<int>& ids) {
    std::vector<P2> p;
    for (int i : ids) p.push_back(fr.to(m.verts[i]));
    return p;
}

// Where a point is on a surface: inside, on its outline (edge k from outer[k], or a
// corner k), or on / in a hole (counts as outside, `onHole` set).
FacePlace placeOnSurface(const Mesh& m, const Surface& s, const Vec3& p, int* index = nullptr, bool* onHole = nullptr) {
    if (onHole) *onHole = false;
    if (std::fabs(dot(p - s.fr.o, s.fr.n)) > kOn) return FacePlace::Outside;
    const P2 q = s.fr.to(p);
    const std::vector<P2> o = positions(m, s.fr, s.outer);
    const int n = (int)o.size();
    for (int k = 0; k < n; ++k)
        if (len(q - o[k]) <= kOn) {
            if (index) *index = k;
            return FacePlace::OnCorner;
        }
    for (int k = 0; k < n; ++k)
        if (segPointDist(q, o[k], o[(k + 1) % n]) <= kOn) {
            if (index) *index = k;
            return FacePlace::OnEdge;
        }
    for (const auto& h : s.holes) {
        const std::vector<P2> hp = positions(m, s.fr, h);
        bool on = false;
        for (size_t k = 0; k < hp.size() && !on; ++k) on = segPointDist(q, hp[k], hp[(k + 1) % hp.size()]) <= kOn;
        if (on || pointInPolygon(q, hp)) {
            if (onHole) *onHole = true;
            return FacePlace::Outside;
        }
    }
    return pointInPolygon(q, o) ? FacePlace::Inside : FacePlace::Outside;
}

// An outline (counter-clockwise) with holes (clockwise), joined by bridges into one
// list (a bridge's ends are visited twice) and split into faces of 3 or 4 corners.
bool splitWithHoles(std::vector<int> ids, std::vector<P2> pos, const std::vector<std::vector<int>>& holeIds,
                    const std::vector<std::vector<P2>>& holePos, std::vector<std::vector<int>>& out) {
    std::vector<size_t> order(holeIds.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    auto maxX = [&](size_t h) {
        double m = -1e300;
        for (const P2& q : holePos[h]) m = std::max(m, q.x);
        return m;
    };
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return maxX(a) > maxX(b); });
    std::vector<bool> merged(holeIds.size(), false);
    for (size_t h : order) {
        const auto& hp = holePos[h];
        double bestL = 1e300;
        int bestJ = -1, bestK = -1;
        for (size_t k = 0; k < hp.size(); ++k)
            for (size_t j = 0; j < pos.size(); ++j) {
                const P2 a = pos[j], b = hp[k];
                const double l = len(a - b);
                if (l >= bestL) continue;
                bool bad = false;
                for (size_t q = 0; q < pos.size() && !bad; ++q) bad = segmentsCross(a, b, pos[q], pos[(q + 1) % pos.size()]);
                for (size_t g = 0; g < holeIds.size() && !bad; ++g) {
                    if (merged[g]) continue;
                    const auto& gp = holePos[g];
                    for (size_t q = 0; q < gp.size() && !bad; ++q) bad = segmentsCross(a, b, gp[q], gp[(q + 1) % gp.size()]);
                }
                const P2 mid{(a.x + b.x) / 2, (a.y + b.y) / 2};
                for (size_t g = 0; g < holeIds.size() && !bad; ++g)
                    if (pointInPolygon(mid, holePos[g])) bad = true;
                if (!bad) bestL = l, bestJ = (int)j, bestK = (int)k;
            }
        if (bestJ < 0) return false;
        std::vector<int> nid(ids.begin(), ids.begin() + bestJ + 1);
        std::vector<P2> npos(pos.begin(), pos.begin() + bestJ + 1);
        const size_t hn = hp.size();
        for (size_t st = 0; st <= hn; ++st) {
            const size_t k = (bestK + st) % hn;
            nid.push_back(holeIds[h][k]);
            npos.push_back(hp[k]);
        }
        nid.push_back(ids[bestJ]);
        npos.push_back(pos[bestJ]);
        nid.insert(nid.end(), ids.begin() + bestJ + 1, ids.end());
        npos.insert(npos.end(), pos.begin() + bestJ + 1, pos.end());
        ids = nid;
        pos = npos;
        merged[h] = true;
    }
    std::vector<int> local(ids.size());
    for (size_t i = 0; i < ids.size(); ++i) local[i] = (int)i;
    auto pieces = splitPoly2D(local, pos);
    if (pieces.empty()) return false;
    out.clear();
    for (auto& p : pieces) {
        std::vector<int> r;
        for (int i : p) r.push_back(ids[i]);
        for (size_t a = 0; a < r.size(); ++a)
            for (size_t b = a + 1; b < r.size(); ++b)
                if (r[a] == r[b]) return false;
        out.push_back(r);
    }
    return true;
}

struct Cutter {
    MeshEdit& e;
    Surface S;
    Face like;
    int origCount = 0;
    std::string why;
    std::set<int> toDelete;                   // group faces left over (deleted at the end)
    std::map<int, std::vector<int>> inserted; // outline edge k -> new points on it, in order

    P2 to(const Vec3& p) const { return S.fr.to(p); }

    // Replaces the surface's faces by `pieces`: their slots first, then new faces; slots
    // left over are deleted at the end. Returns the pieces' face indices (before that).
    std::vector<int> replaceGroup(const std::vector<std::vector<int>>& pieces) {
        std::vector<int> slots(S.faces.begin(), S.faces.end());
        std::vector<int> out;
        for (size_t i = 0; i < pieces.size(); ++i) {
            const Face f = faceLike(like, pieces[i]);
            if (i < slots.size()) {
                e.setFace(slots[i], f);
                out.push_back(slots[i]);
            } else {
                out.push_back(e.addFace(f));
            }
        }
        for (size_t i = pieces.size(); i < slots.size(); ++i) toDelete.insert(slots[i]);
        return out;
    }

    // A point on the outline: its index (an existing point, or a new one on edge k).
    int borderPoint(const Vec3& p) {
        int k = -1;
        const FacePlace at = placeOnSurface(e.mesh(), S, p, &k);
        if (at == FacePlace::OnCorner) return S.outer[k];
        if (at != FacePlace::OnEdge) return -1;
        const int n = (int)S.outer.size();
        const Vec3 a = e.mesh().verts[S.outer[k]], c = e.mesh().verts[S.outer[(k + 1) % n]];
        const Vec3 d = c - a;
        const double t = std::clamp(dot(p - a, d) / dot(d, d), 0.0, 1.0);
        const int v = e.addVert(a + d * t);
        auto& list = inserted[k];
        list.push_back(v);
        std::sort(list.begin(), list.end(),
                  [&](int x, int y) { return dot(e.mesh().verts[x] - a, d) < dot(e.mesh().verts[y] - a, d); });
        return v;
    }

    std::vector<int> outerWithInserted() const {
        std::vector<int> loop;
        for (int k = 0; k < (int)S.outer.size(); ++k) {
            loop.push_back(S.outer[k]);
            auto it = inserted.find(k);
            if (it != inserted.end()) loop.insert(loop.end(), it->second.begin(), it->second.end());
        }
        return loop;
    }

    // Faces outside the surface sharing an outline edge with new points get them too.
    bool fixNeighbours() {
        const Mesh& m = e.mesh();
        const int n = (int)S.outer.size();
        std::map<int, std::map<int, std::vector<int>>> todo;
        for (const auto& [k, pts] : inserted) {
            const int a = S.outer[k], b = S.outer[(k + 1) % n];
            for (int g = 0; g < origCount; ++g) {
                if (S.faces.count(g)) continue;
                const Face& gf = m.faces[g];
                for (int j = 0; j < gf.n; ++j)
                    if (gf.v[j] == b && gf.v[(j + 1) % gf.n] == a) todo[g][j] = std::vector<int>(pts.rbegin(), pts.rend());
            }
        }
        for (const auto& [g, edges] : todo) {
            const Face gf = e.mesh().faces[g];
            std::vector<int> loop;
            for (int q = 0; q < gf.n; ++q) {
                loop.push_back(gf.v[q]);
                auto it = edges.find(q);
                if (it != edges.end()) loop.insert(loop.end(), it->second.begin(), it->second.end());
            }
            auto pieces = splitPolygon(e.mesh(), loop, faceNormal(e.mesh(), g));
            if (!piecesOk(e.mesh(), pieces)) {
                why = "a face next to it can't take the new point";
                return false;
            }
            replaceFace(e, g, pieces);
        }
        return true;
    }

    // Splits the surface (no holes) along a path from one point of its outline to another.
    bool lineCut(const std::vector<Vec3>& path, std::vector<int>& facesA, std::vector<int>& facesB,
                 std::vector<P2>& loopA2, std::vector<P2>& loopB2) {
        if (path.size() < 2) {
            why = "a line needs two points";
            return false;
        }
        const std::vector<P2> o2 = positions(e.mesh(), S.fr, S.outer);
        std::vector<P2> p2;
        for (const Vec3& p : path) p2.push_back(to(p));
        for (size_t i = 1; i + 1 < path.size(); ++i)
            if (placeOnSurface(e.mesh(), S, path[i]) != FacePlace::Inside) {
                why = "the line has to stay inside the face";
                return false;
            }
        for (size_t i = 0; i + 1 < p2.size(); ++i) {
            if (len(p2[i + 1] - p2[i]) < kOn) {
                why = "two points of the line are on top of each other";
                return false;
            }
            for (size_t k = 0; k < o2.size(); ++k)
                if (segmentsCross(p2[i], p2[i + 1], o2[k], o2[(k + 1) % o2.size()])) {
                    why = "the line goes over the edge of the face";
                    return false;
                }
            for (size_t j = i + 2; j + 1 < p2.size(); ++j)
                if (segmentsCross(p2[i], p2[i + 1], p2[j], p2[j + 1])) {
                    why = "the line crosses itself";
                    return false;
                }
            for (const auto& h : S.holes) {
                const std::vector<P2> hp = positions(e.mesh(), S.fr, h);
                for (size_t k = 0; k < hp.size(); ++k)
                    if (segmentsCross(p2[i], p2[i + 1], hp[k], hp[(k + 1) % hp.size()])) {
                        why = "the line crosses a hole";
                        return false;
                    }
            }
            const Vec3 mid = (path[i] + path[i + 1]) * 0.5;
            if (placeOnSurface(e.mesh(), S, mid) != FacePlace::Inside) {
                why = "the line runs along the edge of the face";
                return false;
            }
        }
        const int A = borderPoint(path.front()), B = borderPoint(path.back());
        if (A < 0 || B < 0) {
            why = "a line must start and end on an edge of the face";
            return false;
        }
        if (A == B) {
            why = "the line starts and ends at the same point";
            return false;
        }
        std::vector<int> mid;
        for (size_t i = 1; i + 1 < path.size(); ++i) mid.push_back(e.addVert(path[i]));
        const std::vector<int> outer = outerWithInserted();
        const int n = (int)outer.size();
        const int pa = (int)(std::find(outer.begin(), outer.end(), A) - outer.begin());
        const int pb = (int)(std::find(outer.begin(), outer.end(), B) - outer.begin());
        if (pa >= n || pb >= n) {
            why = "the line's ends aren't on the face's edge";
            return false;
        }
        std::vector<int> la, lb;
        for (int i = pa;; i = (i + 1) % n) {
            la.push_back(outer[i]);
            if (i == pb) break;
        }
        la.insert(la.end(), mid.rbegin(), mid.rend());
        for (int i = pb;; i = (i + 1) % n) {
            lb.push_back(outer[i]);
            if (i == pa) break;
        }
        lb.insert(lb.end(), mid.begin(), mid.end());
        loopA2 = positions(e.mesh(), S.fr, la);
        loopB2 = positions(e.mesh(), S.fr, lb);
        // The surface's holes go with the side they lie in.
        std::vector<std::vector<int>> ha, hb;
        std::vector<std::vector<P2>> pa2, pb2;
        for (const auto& h : S.holes) {
            const std::vector<P2> hp = positions(e.mesh(), S.fr, h);
            if (pointInPolygon(hp[0], loopA2)) ha.push_back(h), pa2.push_back(hp);
            else hb.push_back(h), pb2.push_back(hp);
        }
        std::vector<std::vector<int>> sa, sb;
        if (ha.empty()) sa = splitPolygon(e.mesh(), la, S.fr.n);
        else if (!splitWithHoles(la, loopA2, ha, pa2, sa)) sa.clear();
        if (hb.empty()) sb = splitPolygon(e.mesh(), lb, S.fr.n);
        else if (!splitWithHoles(lb, loopB2, hb, pb2, sb)) sb.clear();
        if (!piecesOk(e.mesh(), sa) || !piecesOk(e.mesh(), sb)) {
            why = "it would leave a sliver of a face (too close to a corner or an edge)";
            return false;
        }
        std::vector<std::vector<int>> all = sa;
        all.insert(all.end(), sb.begin(), sb.end());
        const std::vector<int> made = replaceGroup(all);
        facesA.assign(made.begin(), made.begin() + sa.size());
        facesB.assign(made.begin() + sa.size(), made.end());
        return fixNeighbours();
    }

    // A closed shape inside the surface: it becomes a hole of the surface, filled by its
    // own faces.
    bool insideCut(const std::vector<Vec3>& pts, std::vector<int>& shapeFaces) {
        const Mesh& m0 = e.mesh();
        std::vector<P2> s2;
        for (const Vec3& p : pts) s2.push_back(to(p));
        // Its outline mustn't cross a hole's or go round one.
        for (const auto& h : S.holes) {
            const std::vector<P2> hp = positions(m0, S.fr, h);
            for (size_t i = 0; i < s2.size(); ++i)
                for (size_t k = 0; k < hp.size(); ++k)
                    if (segmentsCross(s2[i], s2[(i + 1) % s2.size()], hp[k], hp[(k + 1) % hp.size()])) {
                        why = "the shape crosses a hole";
                        return false;
                    }
            if (pointInPolygon(hp[0], s2)) {
                why = "the shape goes round a hole";
                return false;
            }
        }
        const std::vector<P2> o2 = positions(m0, S.fr, S.outer);
        for (size_t i = 0; i < s2.size(); ++i)
            for (size_t k = 0; k < o2.size(); ++k)
                if (segmentsCross(s2[i], s2[(i + 1) % s2.size()], o2[k], o2[(k + 1) % o2.size()])) {
                    why = "the shape goes over the edge of the face";
                    return false;
                }
        std::vector<int> shape;
        for (const Vec3& p : pts) shape.push_back(e.addVert(p));
        std::vector<std::vector<int>> holeIds = S.holes;
        std::vector<std::vector<P2>> holePos;
        for (const auto& h : S.holes) holePos.push_back(positions(e.mesh(), S.fr, h));
        holeIds.push_back(std::vector<int>(shape.rbegin(), shape.rend()));
        holePos.push_back(std::vector<P2>(s2.rbegin(), s2.rend()));
        std::vector<std::vector<int>> around;
        if (!splitWithHoles(S.outer, positions(e.mesh(), S.fr, S.outer), holeIds, holePos, around) ||
            !piecesOk(e.mesh(), around)) {
            why = "the shape can't be cut into this face";
            return false;
        }
        auto inner = splitPolygon(e.mesh(), shape, S.fr.n);
        if (!piecesOk(e.mesh(), inner)) {
            why = "the shape is too small";
            return false;
        }
        std::vector<std::vector<int>> all = inner;
        all.insert(all.end(), around.begin(), around.end());
        const std::vector<int> made = replaceGroup(all);
        shapeFaces.assign(made.begin(), made.begin() + inner.size());
        return true;
    }
};

} // namespace

bool cutShape(MeshEdit& e, int face, const std::vector<Vec3>& ptsIn, bool closed, std::set<int>* shape, std::string* why,
              bool wholeSurface) {
    auto fail = [&](const std::string& w) {
        if (why) *why = w;
        return false;
    };
    if (face < 0 || face >= (int)e.mesh().faces.size()) return fail("no face there");
    Cutter c{e, {}, e.mesh().faces[face], (int)e.mesh().faces.size(), {}, {}, {}};
    if (wholeSurface) {
        if (!surfaceOf(e.mesh(), face, c.S)) return fail("that face can't be drawn on");
    } else {
        Vec3 o, u, v, n;
        if (!faceFrame(e.mesh(), face, o, u, v, n)) return fail("that face has no area");
        c.S.fr = Frame{o, u, v, n};
        c.S.faces = {face};
        const Face& f = e.mesh().faces[face];
        for (int k = 0; k < f.n; ++k) c.S.outer.push_back(f.v[k]);
    }
    const Vec3 o = c.S.fr.o, n = c.S.fr.n;
    std::vector<Vec3> pts;
    for (const Vec3& p : ptsIn) {
        const Vec3 q = clean(p - n * dot(p - o, n));
        if (!pts.empty() && length(q - pts.back()) < kOn) continue;
        pts.push_back(q);
    }
    if (closed && pts.size() > 1 && length(pts.front() - pts.back()) < kOn) pts.pop_back();
    std::vector<FacePlace> place;
    for (const Vec3& p : pts) {
        bool onHole = false;
        place.push_back(placeOnSurface(e.mesh(), c.S, p, nullptr, &onHole));
        if (onHole) return fail("the shape touches a hole");
        if (place.back() == FacePlace::Outside) return fail("the shape goes over the edge of the face");
    }
    std::vector<int> picked;
    if (!closed) {
        std::vector<int> a, b;
        std::vector<P2> la, lb;
        if (!c.lineCut(pts, a, b, la, lb)) return fail(c.why);
        picked = std::fabs(area2(la)) <= std::fabs(area2(lb)) ? a : b;
    } else {
        if (pts.size() < 3) return fail("the shape is too small");
        std::vector<P2> s2;
        for (const Vec3& p : pts) s2.push_back(c.to(p));
        if (std::fabs(area2(s2)) < kMinArea) return fail("the shape is too small");
        if (area2(s2) < 0) {
            std::reverse(pts.begin(), pts.end());
            std::reverse(place.begin(), place.end());
            std::reverse(s2.begin(), s2.end());
        }
        const int k = (int)pts.size();
        int inside = 0;
        for (FacePlace p : place) inside += p == FacePlace::Inside;
        if (inside == k) {
            std::vector<int> f;
            if (!c.insideCut(pts, f)) return fail(c.why);
            picked = f;
        } else {
            if (inside == 0) return fail("the shape lies along the edge of the face");
            int start = -1, runs = 0;
            for (int i = 0; i < k; ++i)
                if (place[i] == FacePlace::Inside && place[(i + k - 1) % k] != FacePlace::Inside) start = i, ++runs;
            if (runs != 1) return fail("the shape touches the edge of the face in more than one place");
            std::vector<Vec3> path = {pts[(start + k - 1) % k]};
            int i = start;
            while (place[i] == FacePlace::Inside) {
                path.push_back(pts[i]);
                i = (i + 1) % k;
            }
            path.push_back(pts[i]);
            std::vector<int> a, b;
            std::vector<P2> la, lb;
            if (!c.lineCut(path, a, b, la, lb)) return fail(c.why);
            P2 mid{0, 0};
            for (const P2& q : s2) mid.x += q.x / k, mid.y += q.y / k;
            picked = pointInPolygon(mid, la) ? a : b;
        }
    }
    // Faces left over from the surface go; the picked ones are renumbered to match.
    if (!c.toDelete.empty()) {
        std::vector<int> map;
        e.deleteFaces(c.toDelete, &map);
        for (int& f : picked) f = map[f];
    }
    if (shape) shape->insert(picked.begin(), picked.end());
    return true;
}

// ---------------------------------------------------------------- holes through


namespace {

// A face at right angles to a doorway (its floor, or a side for a shape in a corner):
// the flat, joined faces around `face` are merged, the strip between a-b and its far
// copy fa-fb taken out, and the two parts left rebuilt. Faces to delete go into `gone`.
bool openSide(MeshEdit& e, int face, int a, int b, int fa, int fb, std::set<int>& gone) {
    const Mesh& m = e.mesh();
    const Vec3 n = faceNormal(m, face), c = faceCentre(m, face);
    // The flat group of faces around it.
    std::set<int> group = {face};
    std::vector<int> todo = {face};
    while (!todo.empty()) {
        const int f = todo.back();
        todo.pop_back();
        const Face& ff = m.faces[f];
        for (int g = 0; g < (int)m.faces.size(); ++g) {
            if (group.count(g) || gone.count(g)) continue;
            if (dot(faceNormal(m, g), n) < 1 - 1e-9 || std::fabs(dot(faceCentre(m, g) - c, n)) > 1e-6) continue;
            const Face& gf = m.faces[g];
            bool shares = false;
            for (int i = 0; i < ff.n && !shares; ++i)
                for (int j = 0; j < gf.n; ++j)
                    if (ff.v[i] == gf.v[(j + 1) % gf.n] && ff.v[(i + 1) % ff.n] == gf.v[j]) shares = true;
            if (shares) {
                group.insert(g);
                todo.push_back(g);
            }
        }
    }
    const std::vector<int> U = outlineLoop(m, group);
    if (U.empty()) return false;
    const int un = (int)U.size();
    auto at = [&](int v) { return (int)(std::find(U.begin(), U.end(), v) - U.begin()); };
    const int pb = at(b), pa = at(a), pfa = at(fa), pfb = at(fb);
    if (pa >= un || pb >= un || pfa >= un || pfb >= un) return false;
    if ((pb + 1) % un != pa) return false; // b -> a is the doorway's edge in this face
    // Walking on from a, the first of fa / fb met ends the first part; the other starts
    // the second part, which ends at b.
    std::vector<int> p1, p2;
    int i = pa;
    while (U[i] != fa && U[i] != fb) {
        p1.push_back(U[i]);
        i = (i + 1) % un;
        if (i == pa) return false;
    }
    p1.push_back(U[i]);
    const int other = U[i] == fa ? fb : fa;
    if ((i + 1) % un != at(other)) return false; // the far edge must be next
    i = at(other);
    while (true) {
        p2.push_back(U[i]);
        if (U[i] == b) break;
        i = (i + 1) % un;
        if (i == at(other)) return false;
    }
    std::vector<std::vector<int>> pieces;
    for (const auto& p : {p1, p2}) {
        if (p.size() < 3) continue; // the opening reaches the corner: nothing left there
        auto s = splitPolygon(m, p, n);
        if (!piecesOk(m, s)) return false;
        pieces.insert(pieces.end(), s.begin(), s.end());
    }
    const Face like = m.faces[face];
    for (const auto& p : pieces) e.addFace(faceLike(like, p));
    gone.insert(group.begin(), group.end());
    return true;
}

} // namespace

double throughDistance(const Mesh& m, const std::set<int>& region, int* target) {
    if (region.empty()) return 0;
    const Vec3 n = faceNormal(m, *region.begin());
    if (length(n) < 0.5) return 0;
    for (int f : region)
        if (dot(faceNormal(m, f), n) < 1 - 1e-9) return 0;
    const std::vector<int> loop = outlineLoop(m, region);
    if (loop.size() < 3) return 0;
    // Drawn on a face: every edge of its outline is shared with another face, in the same
    // plane (inside the face) or at right angles (the face's border: a doorway).
    const Vec3 c = faceCentre(m, *region.begin());
    int inner = 0;
    for (size_t i = 0; i < loop.size(); ++i) {
        const int a = loop[i], b = loop[(i + 1) % loop.size()];
        int kind = 0; // 1 same plane, 2 at right angles
        for (int g = 0; g < (int)m.faces.size() && !kind; ++g) {
            if (region.count(g)) continue;
            const Face& f = m.faces[g];
            for (int k = 0; k < f.n; ++k)
                if (f.v[k] == b && f.v[(k + 1) % f.n] == a) {
                    const Vec3 gn = faceNormal(m, g);
                    if (dot(gn, n) > 1 - 1e-9 && std::fabs(dot(faceCentre(m, g) - c, n)) < 1e-6) kind = 1;
                    else if (std::fabs(dot(gn, n)) < 1e-9) kind = 2;
                    else kind = 3;
                    break;
                }
        }
        if (kind == 0 || kind == 3) return 0;
        inner += kind == 1;
    }
    if (inner == 0) return 0;
    // The nearest face behind it, facing the other way, that the whole outline fits inside.
    double best = 0;
    for (int g = 0; g < (int)m.faces.size(); ++g) {
        if (region.count(g) || dot(faceNormal(m, g), n) > -1 + 1e-9) continue;
        const double t = dot(c - m.verts[m.faces[g].v[0]], n);
        if (t <= 1e-6 || (best > 0 && t >= best)) continue;
        Surface far;
        if (!surfaceOf(m, g, far)) continue;
        bool inside = true;
        for (int v : loop)
            if (placeOnSurface(m, far, m.verts[v] - n * t) == FacePlace::Outside) {
                inside = false;
                break;
            }
        if (!inside) continue;
        best = t;
        if (target) *target = g;
    }
    return best;
}

bool pushThrough(MeshEdit& e, const std::set<int>& region, std::string* why) {
    auto fail = [&](const std::string& w) {
        if (why) *why = w;
        return false;
    };
    int target = -1;
    const double t = throughDistance(e.mesh(), region, &target);
    if (t <= 0) return fail("nothing on the other side to go through to");
    const Vec3 n = faceNormal(e.mesh(), *region.begin());
    const std::vector<int> loop = outlineLoop(e.mesh(), region);
    std::vector<Vec3> far;
    for (int v : loop) far.push_back(e.mesh().verts[v] - n * t);
    std::set<int> farShape;
    std::string w;
    if (!cutShape(e, target, far, true, &farShape, &w)) return fail(w);
    // The far copies of the outline's points (cutShape cleaned them).
    std::vector<int> farIdx;
    for (const Vec3& p : far) {
        const Vec3 q = clean(p);
        int found = -1;
        for (int fi : farShape) {
            const Face& f = e.mesh().faces[fi];
            for (int k = 0; k < f.n; ++k)
                if (length(e.mesh().verts[f.v[k]] - q) < 2 * kOn) found = f.v[k];
        }
        if (found < 0) return fail("the far side didn't line up");
        farIdx.push_back(found);
    }
    // Walls: from the shape's edge a -> b to the far side, facing into the hole. An edge on
    // the face's border (a doorway's sill) instead opens the face at right angles there:
    // the strip between a-b and its far copy goes.
    const Face like = e.mesh().faces[*region.begin()];
    std::set<int> gone = region;
    gone.insert(farShape.begin(), farShape.end());
    std::vector<std::vector<int>> sides; // faces at right angles to rebuild
    for (size_t i = 0; i < loop.size(); ++i) {
        const size_t j = (i + 1) % loop.size();
        const int a = loop[i], b = loop[j];
        int across = -1;
        for (int g = 0; g < (int)e.mesh().faces.size() && across < 0; ++g) {
            if (gone.count(g)) continue;
            const Face& f = e.mesh().faces[g];
            for (int k = 0; k < f.n; ++k)
                if (f.v[k] == b && f.v[(k + 1) % f.n] == a) across = g;
        }
        if (across >= 0 && std::fabs(dot(faceNormal(e.mesh(), across), n)) < 1e-9) {
            if (!openSide(e, across, a, b, farIdx[i], farIdx[j], gone)) return fail("the floor of the opening can't be cut");
            continue;
        }
        e.addFace(faceLike(like, {a, b, farIdx[j], farIdx[i]}));
    }
    e.deleteFaces(gone);
    return true;
}

// ---------------------------------------------------------------- outlines

bool makeOutline(MeshEdit& e, const std::vector<Vec3>& outerIn, const std::vector<std::vector<Vec3>>& holesIn,
                 const Vec3& normal, bool twoSided, std::set<int>* faces, std::string* why) {
    auto fail = [&](const std::string& w) {
        if (why) *why = w;
        return false;
    };
    if (outerIn.size() < 3) return fail("an outline needs three points");
    const Frame fr = frameFor(outerIn[0], normal);
    auto flat = [&](const std::vector<Vec3>& in) {
        std::vector<Vec3> out;
        for (const Vec3& p : in) {
            const Vec3 q = clean(p - normal * dot(p - outerIn[0], normal));
            if (!out.empty() && length(q - out.back()) < kOn) continue;
            out.push_back(q);
        }
        if (out.size() > 1 && length(out.front() - out.back()) < kOn) out.pop_back();
        return out;
    };
    auto to2 = [&](const std::vector<Vec3>& in) {
        std::vector<P2> o;
        for (const Vec3& p : in) o.push_back(fr.to(p));
        return o;
    };
    auto simple = [&](const std::vector<P2>& p) {
        const size_t n = p.size();
        for (size_t i = 0; i < n; ++i)
            for (size_t j = i + 1; j < n; ++j) {
                if (j == i + 1 || (i == 0 && j == n - 1)) continue;
                if (segmentsCross(p[i], p[(i + 1) % n], p[j], p[(j + 1) % n])) return false;
            }
        for (size_t i = 0; i < n; ++i)
            for (size_t j = i + 1; j < n; ++j)
                if (len(p[i] - p[j]) < kOn) return false;
        return true;
    };
    auto crossesLoop = [&](const std::vector<P2>& a, const std::vector<P2>& b) {
        for (size_t i = 0; i < a.size(); ++i)
            for (size_t j = 0; j < b.size(); ++j)
                if (segmentsCross(a[i], a[(i + 1) % a.size()], b[j], b[(j + 1) % b.size()])) return true;
        return false;
    };
    std::vector<Vec3> outer = flat(outerIn);
    if (outer.size() < 3) return fail("an outline needs three points");
    std::vector<P2> o2 = to2(outer);
    if (!simple(o2)) return fail("the outline crosses itself");
    if (std::fabs(area2(o2)) < kMinPiece) return fail("the outline is too small");
    if (area2(o2) < 0) {
        std::reverse(outer.begin(), outer.end());
        std::reverse(o2.begin(), o2.end());
    }
    std::vector<std::vector<Vec3>> holes;
    std::vector<std::vector<P2>> h2s;
    for (const auto& hin : holesIn) {
        std::vector<Vec3> h = flat(hin);
        if (h.size() < 3) continue;
        std::vector<P2> h2 = to2(h);
        if (!simple(h2)) return fail("a hole crosses itself");
        if (std::fabs(area2(h2)) < kMinPiece) return fail("a hole is too small");
        if (area2(h2) > 0) { // holes go clockwise
            std::reverse(h.begin(), h.end());
            std::reverse(h2.begin(), h2.end());
        }
        for (const P2& q : h2)
            if (!pointInPolygon(q, o2)) return fail("a hole has to be inside the outline");
        if (crossesLoop(h2, o2)) return fail("a hole crosses the outline");
        for (const auto& other : h2s) {
            if (crossesLoop(h2, other)) return fail("two holes cross");
            if (pointInPolygon(h2[0], other) || pointInPolygon(other[0], h2)) return fail("a hole is inside another hole");
        }
        holes.push_back(h);
        h2s.push_back(h2);
    }
    // Points, then the holes joined to the outline by bridges (the bridge's two ends
    // are visited twice), then split into faces.
    std::vector<int> ids;
    std::vector<P2> pos;
    for (size_t i = 0; i < outer.size(); ++i) {
        ids.push_back(e.addVert(outer[i]));
        pos.push_back(o2[i]);
    }
    std::vector<std::vector<int>> holeIds;
    for (const auto& h : holes) {
        std::vector<int> hi;
        for (const Vec3& p : h) hi.push_back(e.addVert(p));
        holeIds.push_back(hi);
    }
    std::vector<std::vector<int>> real;
    if (!splitWithHoles(ids, pos, holeIds, h2s, real)) return fail("the outline can't be made into faces");
    if (!piecesOk(e.mesh(), real)) return fail("it would leave a sliver of a face");
    Face like;
    like.twoSided = twoSided;
    std::set<int> made;
    for (const auto& r : real) made.insert(e.addFace(faceLike(like, r)));
    if (faces) *faces = made;
    return true;
}

} // namespace mazz
