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
double turn(P2 a, P2 b, P2 c) {
    const P2 e1 = b - a, e2 = c - b;
    const double l = len(e1) * len(e2);
    if (l <= 0) return 0;
    const double s = cross(e1, e2) / l;
    return std::fabs(s) < kStraight ? 0 : s;
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
    while (L.size() > 3 && guard++ < 10000) {
        bool clipped = false;
        const int m = (int)L.size();
        for (int k = 0; k < m; ++k) {
            const int a = L[(k + m - 1) % m], b = L[k], c = L[(k + 1) % m];
            if (turn(pts[a], pts[b], pts[c]) <= 0) continue;
            bool blocked = false;
            for (int q : L)
                if (q != a && q != b && q != c && inTriangle(pts[q], pts[a], pts[b], pts[c])) {
                    blocked = true;
                    break;
                }
            if (blocked) continue;
            tris.push_back({a, b, c});
            L.erase(L.begin() + k);
            clipped = true;
            break;
        }
        if (!clipped) return {};
    }
    if (L.size() == 3 && turn(pts[L[0]], pts[L[1]], pts[L[2]]) > 0) tris.push_back({L[0], L[1], L[2]});
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

// ---------------------------------------------------------------- cutting

namespace {

struct Cutter {
    MeshEdit& e;
    int face;
    Frame fr;
    Face orig;
    std::vector<P2> outer2; // the face's corners in its frame
    int origCount = 0;      // faces in the model before the cut
    std::string why;

    P2 to(const Vec3& p) const { return fr.to(p); }

    // A point on the face's border: its index (an existing corner, or a new point
    // that `insert` records on edge k).
    struct Border {
        int vert = -1;
        int edge = -1; // the edge it was put on (-1: a corner)
    };
    std::map<int, std::vector<int>> inserted; // edge -> new points on it, in order along it

    Border borderPoint(const Vec3& p) {
        int edge = -1, corner = -1;
        const FacePlace at = placeOnFace(e.mesh(), face, p, &edge, &corner);
        Border b;
        if (at == FacePlace::OnCorner) {
            b.vert = orig.v[corner];
            return b;
        }
        if (at != FacePlace::OnEdge) return b;
        // Exactly on the edge's line (the click was snapped near it).
        const Vec3 a = e.mesh().verts[orig.v[edge]], c = e.mesh().verts[orig.v[(edge + 1) % orig.n]];
        const Vec3 d = c - a;
        const double t = std::clamp(dot(p - a, d) / dot(d, d), 0.0, 1.0);
        b.vert = e.addVert(a + d * t);
        b.edge = edge;
        auto& list = inserted[edge];
        list.push_back(b.vert);
        std::sort(list.begin(), list.end(), [&](int x, int y) {
            return dot(e.mesh().verts[x] - a, d) < dot(e.mesh().verts[y] - a, d);
        });
        return b;
    }

    // The face's outline with the new border points.
    std::vector<int> outerLoop() const {
        std::vector<int> loop;
        for (int k = 0; k < orig.n; ++k) {
            loop.push_back(orig.v[k]);
            auto it = inserted.find(k);
            if (it != inserted.end()) loop.insert(loop.end(), it->second.begin(), it->second.end());
        }
        return loop;
    }

    // Faces next to the cut face get the new points on their shared edge.
    bool fixNeighbours() {
        for (const auto& [k, pts] : inserted) {
            const int a = orig.v[k], b = orig.v[(k + 1) % orig.n];
            const Mesh& m = e.mesh();
            const int count = (int)m.faces.size(); // faces added below are pieces already done
            for (int g = 0; g < count; ++g) {
                if (g == face || g >= origCount) continue;
                const Face gf = m.faces[g];
                for (int j = 0; j < gf.n; ++j) {
                    if (gf.v[j] != b || gf.v[(j + 1) % gf.n] != a) continue;
                    std::vector<int> loop;
                    for (int q = 0; q < gf.n; ++q) {
                        loop.push_back(gf.v[q]);
                        if (q == j) loop.insert(loop.end(), pts.rbegin(), pts.rend()); // b -> a: reversed
                    }
                    const Vec3 gn = faceNormal(m, g);
                    auto pieces = splitPolygon(m, loop, gn);
                    if (!piecesOk(m, pieces)) {
                        why = "a face next to it can't take the new point";
                        return false;
                    }
                    replaceFace(e, g, pieces);
                    break;
                }
            }
        }
        return true;
    }

    std::vector<P2> loop2(const std::vector<int>& loop) const {
        std::vector<P2> p;
        for (int i : loop) p.push_back(to(e.mesh().verts[i]));
        return p;
    }

    // Splits the face along a path from one border point to another through the inside.
    // Returns the two loops' faces (the first is to the left of the path's direction... see below).
    bool lineCut(const std::vector<Vec3>& path, std::vector<int>& facesA, std::vector<int>& facesB,
                 std::vector<P2>& loopA2, std::vector<P2>& loopB2) {
        if (path.size() < 2) {
            why = "a line needs two points";
            return false;
        }
        // The inside points must be inside; the path mustn't cross the face's border or itself.
        std::vector<P2> p2;
        for (const Vec3& p : path) p2.push_back(to(p));
        for (size_t i = 1; i + 1 < path.size(); ++i)
            if (placeOnFace(e.mesh(), face, path[i]) != FacePlace::Inside) {
                why = "the line has to stay inside the face";
                return false;
            }
        for (size_t i = 0; i + 1 < p2.size(); ++i) {
            if (len(p2[i + 1] - p2[i]) < kOn) {
                why = "two points of the line are on top of each other";
                return false;
            }
            for (size_t k = 0; k < outer2.size(); ++k)
                if (segmentsCross(p2[i], p2[i + 1], outer2[k], outer2[(k + 1) % outer2.size()])) {
                    why = "the line goes over the edge of the face";
                    return false;
                }
            for (size_t j = i + 2; j + 1 < p2.size(); ++j)
                if (segmentsCross(p2[i], p2[i + 1], p2[j], p2[j + 1])) {
                    why = "the line crosses itself";
                    return false;
                }
        }
        // Middle of each straight piece must be inside (not running along the border).
        for (size_t i = 0; i + 1 < path.size(); ++i) {
            const Vec3 mid = (path[i] + path[i + 1]) * 0.5;
            if (placeOnFace(e.mesh(), face, mid) != FacePlace::Inside) {
                why = "the line runs along the edge of the face";
                return false;
            }
        }
        const Border A = borderPoint(path.front()), B = borderPoint(path.back());
        if (A.vert < 0 || B.vert < 0) {
            why = "a line must start and end on an edge of the face";
            return false;
        }
        if (A.vert == B.vert) {
            why = "the line starts and ends at the same point";
            return false;
        }
        std::vector<int> mid;
        for (size_t i = 1; i + 1 < path.size(); ++i) mid.push_back(e.addVert(path[i]));
        const std::vector<int> outer = outerLoop();
        const int n = (int)outer.size();
        const int pa = (int)(std::find(outer.begin(), outer.end(), A.vert) - outer.begin());
        const int pb = (int)(std::find(outer.begin(), outer.end(), B.vert) - outer.begin());
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
        loopA2 = loop2(la);
        loopB2 = loop2(lb);
        auto sa = splitPolygon(e.mesh(), la, fr.n), sb = splitPolygon(e.mesh(), lb, fr.n);
        if (!piecesOk(e.mesh(), sa) || !piecesOk(e.mesh(), sb)) {
            why = "it would leave a sliver of a face (too close to a corner or an edge)";
            return false;
        }
        std::vector<std::vector<int>> all = sa;
        all.insert(all.end(), sb.begin(), sb.end());
        const std::vector<int> made = replaceFace(e, face, all);
        facesA.assign(made.begin(), made.begin() + sa.size());
        facesB.assign(made.begin() + sa.size(), made.end());
        return fixNeighbours();
    }

    // A closed shape strictly inside: the face becomes a frame round it, joined by two
    // bridges into two simple outlines.
    bool ringCut(const std::vector<Vec3>& pts, std::vector<int>& shapeFaces) {
        std::vector<int> inner;
        for (const Vec3& p : pts) inner.push_back(e.addVert(p));
        const std::vector<int> outer = outerLoop();
        const std::vector<P2> o2 = loop2(outer), i2 = loop2(inner);
        const int n = (int)outer.size(), k = (int)inner.size();
        auto crossesAny = [&](P2 a, P2 b, int skipOuter, int skipInner) {
            for (int q = 0; q < n; ++q)
                if (q != skipOuter && (q + 1) % n != skipOuter && segmentsCross(a, b, o2[q], o2[(q + 1) % n])) return true;
            for (int q = 0; q < k; ++q)
                if (q != skipInner && (q + 1) % k != skipInner && segmentsCross(a, b, i2[q], i2[(q + 1) % k])) return true;
            // Not through the shape itself (from a corner across it).
            const P2 mid{(a.x + b.x) / 2, (a.y + b.y) / 2};
            return pointInPolygon(mid, i2);
        };
        struct Bridge {
            int o, i;
            double l;
        };
        std::vector<Bridge> bridges;
        for (int a = 0; a < n; ++a)
            for (int i = 0; i < k; ++i)
                if (!crossesAny(o2[a], i2[i], a, i)) bridges.push_back({a, i, len(o2[a] - i2[i])});
        std::sort(bridges.begin(), bridges.end(), [](const Bridge& x, const Bridge& y) { return x.l < y.l; });
        const Mesh& m = e.mesh();
        for (size_t x = 0; x < bridges.size(); ++x)
            for (size_t y = x + 1; y < bridges.size(); ++y) {
                const Bridge b1 = bridges[x], b2 = bridges[y];
                if (b1.o == b2.o || b1.i == b2.i) continue;
                if (segmentsCross(o2[b1.o], i2[b1.i], o2[b2.o], i2[b2.i])) continue;
                std::vector<int> l1, l2;
                for (int q = b1.o;; q = (q + 1) % n) {
                    l1.push_back(outer[q]);
                    if (q == b2.o) break;
                }
                for (int q = b2.i;; q = (q + k - 1) % k) {
                    l1.push_back(inner[q]);
                    if (q == b1.i) break;
                }
                for (int q = b2.o;; q = (q + 1) % n) {
                    l2.push_back(outer[q]);
                    if (q == b1.o) break;
                }
                for (int q = b1.i;; q = (q + k - 1) % k) {
                    l2.push_back(inner[q]);
                    if (q == b2.i) break;
                }
                auto s1 = splitPolygon(m, l1, fr.n), s2 = splitPolygon(m, l2, fr.n), s3 = splitPolygon(m, inner, fr.n);
                if (!piecesOk(m, s1) || !piecesOk(m, s2) || !piecesOk(m, s3)) continue;
                std::vector<std::vector<int>> all = s3;
                all.insert(all.end(), s1.begin(), s1.end());
                all.insert(all.end(), s2.begin(), s2.end());
                const std::vector<int> made = replaceFace(e, face, all);
                shapeFaces.assign(made.begin(), made.begin() + s3.size());
                return true;
            }
        why = "the shape can't be cut into this face";
        return false;
    }
};

} // namespace

bool cutShape(MeshEdit& e, int face, const std::vector<Vec3>& ptsIn, bool closed, std::set<int>* shape, std::string* why) {
    auto fail = [&](const std::string& w) {
        if (why) *why = w;
        return false;
    };
    Vec3 o, u, v, n;
    if (!faceFrame(e.mesh(), face, o, u, v, n)) return fail("that face has no area");
    Cutter c{e, face, Frame{o, u, v, n}, e.mesh().faces[face], {}, 0, {}, {}};
    for (int k = 0; k < c.orig.n; ++k) c.outer2.push_back(c.to(e.mesh().verts[c.orig.v[k]]));
    c.origCount = (int)e.mesh().faces.size();
    // Into the face's plane, cleaned.
    std::vector<Vec3> pts;
    for (const Vec3& p : ptsIn) {
        const Vec3 q = clean(p - n * dot(p - o, n));
        if (!pts.empty() && length(q - pts.back()) < kOn) continue;
        pts.push_back(q);
    }
    if (closed && pts.size() > 1 && length(pts.front() - pts.back()) < kOn) pts.pop_back();
    std::vector<FacePlace> place;
    for (const Vec3& p : pts) {
        place.push_back(placeOnFace(e.mesh(), face, p));
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
            if (!c.ringCut(pts, f)) return fail(c.why);
            picked = f;
        } else {
            if (inside == 0) return fail("the shape lies along the edge of the face");
            // The inside points must follow each other: one stretch along the border.
            int start = -1, runs = 0;
            for (int i = 0; i < k; ++i)
                if (place[i] == FacePlace::Inside && place[(i + k - 1) % k] != FacePlace::Inside) start = i, ++runs;
            if (runs != 1) return fail("the shape touches the edge of the face in more than one place");
            // Path: the border point before the stretch, the stretch, the border point after.
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
            // The shape is the part its middle lies in.
            P2 mid{0, 0};
            for (const P2& q : s2) mid.x += q.x / k, mid.y += q.y / k;
            picked = pointInPolygon(mid, la) ? a : b;
        }
    }
    if (shape) shape->insert(picked.begin(), picked.end());
    return true;
}

} // namespace mazz
