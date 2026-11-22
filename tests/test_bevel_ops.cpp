// v0.2.6: bevel, bridge, shell (core/bevel_ops). Closed models stay closed.
#include "check.hpp"
#include "core/bevel_ops.hpp"
#include "core/clean_ops.hpp"
#include "core/divide_ops.hpp"
#include "core/draw_ops.hpp"
#include "core/model_ops.hpp"
#include "core/primitives.hpp"
#include <cmath>
#include <cstdio>
#include <random>

using namespace mazz;

namespace {

Mesh box(const Vec3& a, const Vec3& b) {
    Mesh m;
    addBox(m, a, b);
    return m;
}

int vertAt(const Mesh& m, const Vec3& p) {
    for (int i = 0; i < (int)m.verts.size(); ++i)
        if (nearV(m.verts[i], p, 1e-6)) return i;
    return -1;
}

int faceFacing(const Mesh& m, const Vec3& n) {
    for (int i = 0; i < (int)m.faces.size(); ++i)
        if (nearV(faceNormal(m, i), n, 1e-9)) return i;
    return -1;
}

bool good(const Mesh& m) {
    for (int i = 0; i < (int)m.faces.size(); ++i) {
        const Face& f = m.faces[i];
        if (f.n != 3 && f.n != 4) return false;
        for (int a = 0; a < f.n; ++a)
            for (int b = a + 1; b < f.n; ++b)
                if (f.v[a] == f.v[b]) return false;
        if (length(faceNormal(m, i)) == 0) return false;
    }
    for (int v = 0; v < (int)m.verts.size(); ++v) {
        bool used = false;
        for (const Face& f : m.faces)
            for (int k = 0; k < f.n; ++k) used = used || f.v[k] == v;
        if (!used) return false;
    }
    return true;
}

Edge edgeAt(const Mesh& m, const Vec3& a, const Vec3& b) { return makeEdge(vertAt(m, a), vertAt(m, b)); }

void testBevelOneEdge() {
    // The top front edge of a 100 cube, 10 wide: a chamfer. Volume loses a 10x10/2 prism.
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    MeshEdit e(m);
    std::set<int> out;
    std::string why;
    CHECK(bevelEdges(e, {edgeAt(m, {0, 0, 100}, {100, 0, 100})}, {10, 1}, &out, &why));
    CHECK(isClosed(m) && good(m));
    CHECK(near(signedVolume(m), 1e6 - 5000, 1e-6));
    // The two end caps lose a corner (5 corners): each split in two. 6 + 2 + the strip.
    CHECK(m.verts.size() == 10 && m.faces.size() == 9 && out.size() == 1);
    for (const Vec3& p : {Vec3{0, 10, 100}, Vec3{0, 0, 90}, Vec3{100, 10, 100}, Vec3{100, 0, 90}}) CHECK(vertAt(m, p) >= 0);
    CHECK(vertAt(m, {0, 0, 100}) < 0);
    // The new face faces up and forward at 45 degrees.
    const Vec3 n = faceNormal(m, *out.begin());
    CHECK(nearV(n, Vec3{0, -1, 1} * (1 / std::sqrt(2.0)), 1e-9));
    // Two and three segments: a round profile, closed, the arc points 10 from its centre.
    for (int segs : {2, 3}) {
        Mesh r = box({0, 0, 0}, {100, 100, 100});
        MeshEdit er(r);
        CHECK(bevelEdges(er, {edgeAt(r, {0, 0, 100}, {100, 0, 100})}, {10, segs}, &out, &why));
        CHECK(isClosed(r) && good(r) && (int)out.size() == segs);
        int onArc = 0;
        for (const Vec3& p : r.verts)
            if (p.x == 0 && std::abs(length(Vec3{0, p.y - 10, p.z - 90}) - 10) < 2e-3 && p.y < 10 && p.z > 90) ++onArc;
        CHECK(onArc == segs - 1);
        // Volume: the cube less the area between the edge and the arc (polygon), along 100.
        // In the end plane, (y, z): the corner (0, 100), the arc from (10, 100) to (0, 90)
        // round the centre (10, 90).
        std::vector<std::pair<double, double>> poly = {{0, 100}};
        for (int i = 0; i <= segs; ++i) {
            const double t = kPi / 2 * i / segs;
            poly.push_back({10 - 10 * std::sin(t), 90 + 10 * std::cos(t)});
        }
        double area = 0;
        for (size_t i = 0; i < poly.size(); ++i) {
            const auto& a = poly[i];
            const auto& b = poly[(i + 1) % poly.size()];
            area += a.first * b.second - b.first * a.second;
        }
        CHECK(near(signedVolume(r), 1e6 - std::abs(area) / 2 * 100, 1.0));
        // No extra faces at the ends: the caps took the arc points.
        for (int f : out) CHECK(r.faces[f].n == 4);
    }
}

void testBevelCorner() {
    // The three edges at one corner: a triangle where they meet.
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    MeshEdit e(m);
    std::set<int> out;
    std::string why;
    const Vec3 c{0, 0, 0};
    std::set<Edge> eds = {edgeAt(m, c, {100, 0, 0}), edgeAt(m, c, {0, 100, 0}), edgeAt(m, c, {0, 0, 100})};
    CHECK(bevelEdges(e, eds, {10, 1}, &out, &why));
    CHECK(isClosed(m) && good(m));
    CHECK(out.size() == 4); // three strips and the corner triangle
    for (const Vec3& p : {Vec3{10, 10, 0}, Vec3{10, 0, 10}, Vec3{0, 10, 10}}) CHECK(vertAt(m, p) >= 0);
    int tris = 0;
    for (int f : out) tris += m.faces[f].n == 3 ? 1 : 0;
    CHECK(tris == 1);
    // With 2 segments the corner is a small patch of triangles and quads, still closed.
    Mesh r = box({0, 0, 0}, {100, 100, 100});
    MeshEdit er(r);
    eds = {edgeAt(r, c, {100, 0, 0}), edgeAt(r, c, {0, 100, 0}), edgeAt(r, c, {0, 0, 100})};
    CHECK(bevelEdges(er, eds, {10, 2}, &out, &why));
    CHECK(isClosed(r) && good(r) && out.size() > 6);
}

void testBevelAll() {
    // Every edge of a cube, 10 wide: each corner moves in by 10 on two of its axes.
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    std::set<Edge> all;
    for (const Face& f : m.faces)
        for (int k = 0; k < f.n; ++k) all.insert(makeEdge(f.v[k], f.v[(k + 1) % f.n]));
    MeshEdit e(m);
    std::set<int> out;
    std::string why;
    CHECK(bevelEdges(e, all, {10, 1}, &out, &why));
    CHECK(isClosed(m) && good(m));
    CHECK(m.verts.size() == 24 && m.faces.size() == 26 && out.size() == 20);
    for (const Vec3& p : m.verts) {
        int in = 0;
        for (double v : {p.x, p.y, p.z}) in += (v == 10 || v == 90) ? 1 : 0;
        CHECK(in == 2);
    }
    // The top edges as a loop (a rim): closed, top face 80x80.
    Mesh t = box({0, 0, 0}, {100, 100, 100});
    std::set<Edge> topLoop;
    const int top = faceFacing(t, {0, 0, 1});
    for (int k = 0; k < 4; ++k) topLoop.insert(makeEdge(t.faces[top].v[k], t.faces[top].v[(k + 1) % 4]));
    MeshEdit et(t);
    for (int segs : {1, 3}) {
        Mesh u = t;
        MeshEdit eu(u);
        CHECK(bevelEdges(eu, topLoop, {10, segs}, &out, &why));
        CHECK(isClosed(u) && good(u));
        CHECK(vertAt(u, {10, 10, 100}) >= 0 && vertAt(u, {90, 90, 100}) >= 0 && vertAt(u, {0, 0, 90}) >= 0);
    }
}

void testBevelFlatEnd() {
    // An edge in the middle of a flat top (split 2x2) ends where the surface goes on
    // flat: no corner patch, the old point stays on the strip's end. Every segment count.
    for (int segs = 1; segs <= 3; ++segs) {
        Mesh m = box({0, 0, 0}, {100, 100, 100});
        MeshEdit e(m);
        CHECK(subdivide(e, {faceFacing(m, {0, 0, 1})}, 2, 2));
        std::set<int> out;
        std::string why;
        CHECK(bevelEdges(e, {edgeAt(m, {50, 50, 100}, {50, 100, 100})}, {5, segs}, &out, &why));
        CHECK(isClosed(m) && good(m) && near(signedVolume(m), 1e6, 1e-6));
        CHECK(vertAt(m, {50, 50, 100}) >= 0 && vertAt(m, {45, 50, 100}) >= 0 && vertAt(m, {55, 50, 100}) >= 0);
    }
}

void testBevelTooWide() {
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    MeshEdit e(m);
    std::string why;
    CHECK(!bevelEdges(e, {edgeAt(m, {0, 0, 100}, {100, 0, 100})}, {120, 1}, nullptr, &why) && !why.empty());
    // Two opposite edges of the top, 50 each: they would meet. Refused.
    Mesh n = box({0, 0, 0}, {100, 100, 100});
    MeshEdit en(n);
    CHECK(!bevelEdges(en, {edgeAt(n, {0, 0, 100}, {100, 0, 100}), edgeAt(n, {0, 100, 100}, {100, 100, 100})}, {50, 1}, nullptr,
                      &why));
    // Open edges are not beveled (that is Extrude's job).
    Mesh o;
    o.verts = {{0, 0, 0}, {100, 0, 0}, {100, 100, 0}, {0, 100, 0}};
    Face f;
    f.n = 4;
    for (int k = 0; k < 4; ++k) f.v[k] = k;
    o.faces = {f};
    MeshEdit eo(o);
    CHECK(!bevelEdges(eo, {makeEdge(0, 1)}, {10, 1}, nullptr, &why) && !why.empty());
    CHECK(!bevelEdges(eo, {makeEdge(0, 1)}, {0, 1}, nullptr, &why));
}

void testBevelPoints() {
    // A cube corner cut off 10 along each edge: a triangle, volume less 10^3/6.
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    MeshEdit e(m);
    std::set<int> out;
    std::string why;
    CHECK(bevelPoints(e, {vertAt(m, {100, 100, 100})}, 10, &out, &why));
    CHECK(isClosed(m) && good(m) && out.size() == 1 && m.faces[*out.begin()].n == 3);
    CHECK(near(signedVolume(m), 1e6 - 1000.0 / 6, 1e-6));
    CHECK(m.verts.size() == 10);
    // Two corners on one edge, 60 each: they'd cross. Refused.
    Mesh n = box({0, 0, 0}, {100, 100, 100});
    MeshEdit en(n);
    CHECK(!bevelPoints(en, {vertAt(n, {0, 0, 0}), vertAt(n, {100, 0, 0})}, 60, nullptr, &why));
    // All eight: 8 triangles, 6 octagons split into quads and triangles; closed.
    Mesh a = box({0, 0, 0}, {100, 100, 100});
    MeshEdit ea(a);
    std::set<int> all;
    for (int i = 0; i < 8; ++i) all.insert(i);
    CHECK(bevelPoints(ea, all, 20, &out, &why));
    CHECK(isClosed(a) && good(a) && out.size() == 8 && a.verts.size() == 24);
    CHECK(near(signedVolume(a), 1e6 - 8 * 8000.0 / 6, 1e-6));
}

void testBridge() {
    std::string why;
    std::set<int> out;
    // Two boxes side by side with a gap; bridging the facing sides makes one solid.
    Mesh m = box({0, 0, 0}, {40, 40, 40});
    addBox(m, {80, 0, 0}, {120, 40, 40});
    const int a = faceFacing(m, {1, 0, 0}); // the first box's +X side
    int b = -1;
    for (int i = 0; i < (int)m.faces.size(); ++i)
        if (nearV(faceNormal(m, i), {-1, 0, 0}) && faceCentre(m, i).x == 80) b = i;
    MeshEdit e(m);
    CHECK(bridgeFaces(e, {a, b}, {}, &out, &why));
    CHECK(isClosed(m) && good(m) && m.faces.size() == 14 && out.size() == 4);
    CHECK(near(signedVolume(m), 3 * 64000, 1e-6));
    // Segments: rings along the tube.
    Mesh s = box({0, 0, 0}, {40, 40, 40});
    addBox(s, {80, 0, 0}, {120, 40, 40});
    MeshEdit es(s);
    BridgeParams bp;
    bp.segments = 4;
    CHECK(bridgeFaces(es, {a, b}, bp, &out, &why));
    CHECK(isClosed(s) && good(s) && out.size() == 16 && vertAt(s, {50, 0, 0}) >= 0);
    CHECK(near(signedVolume(s), 3 * 64000, 1e-6));
    // A tunnel through a box: front and back faces' centres (inset first).
    Mesh t = box({0, 0, 0}, {100, 100, 100});
    MeshEdit et(t);
    std::set<int> fr, bk;
    InsetParams ip;
    ip.width = 30;
    CHECK(inset(et, {faceFacing(t, {0, -1, 0})}, ip, &fr));
    CHECK(inset(et, {faceFacing(t, {0, 1, 0})}, ip, &bk));
    std::set<int> both = fr;
    both.insert(bk.begin(), bk.end());
    CHECK(bridgeFaces(et, both, {}, &out, &why));
    CHECK(isClosed(t) && good(t) && near(signedVolume(t), 1e6 - 40 * 40 * 100, 1e-6));
    // Not two blocks: refused.
    Mesh v = box({0, 0, 0}, {40, 40, 40});
    MeshEdit ev(v);
    CHECK(!bridgeFaces(ev, {0}, {}, nullptr, &why) && !why.empty());
    CHECK(!bridgeFaces(ev, {faceFacing(v, {0, 0, 1}), faceFacing(v, {1, 0, 0})}, {}, nullptr, &why)); // touching
}

void testBridgeUneven() {
    // A box's side (4 points) to a 6-sided hole: tube of 6 faces, 2 of them triangles.
    Mesh m = box({0, 0, 0}, {40, 40, 40});
    const int base = (int)m.verts.size();
    PrimParams cp;
    cp.kind = PrimKind::Cylinder;
    cp.sides = 6;
    const Mesh c = makePrimitive(cp, {80, 5, 5}, {120, 35, 35}, 0); // along X
    for (const Vec3& p : c.verts) m.verts.push_back(p);
    for (Face f : c.faces) {
        for (int k = 0; k < f.n; ++k) f.v[k] += base;
        m.faces.push_back(f);
    }
    std::string why;
    std::set<int> out;
    int cap = -1;
    for (int i = 0; i < (int)m.faces.size(); ++i)
        if (nearV(faceNormal(m, i), {-1, 0, 0}) && faceCentre(m, i).x < 100 && i >= 6) cap = i;
    if (cap < 0) {
        std::printf("  (no cylinder cap facing -X: uneven bridge skipped)\n");
        return;
    }
    std::set<int> sel = {faceFacing(m, {1, 0, 0})};
    // The cap may be split into several faces: take all facing -X on the cylinder.
    for (int i = 6; i < (int)m.faces.size(); ++i)
        if (nearV(faceNormal(m, i), {-1, 0, 0}, 1e-6)) sel.insert(i);
    MeshEdit e(m);
    CHECK(bridgeFaces(e, sel, {}, &out, &why));
    CHECK(isClosed(m) && good(m) && out.size() == 6);
    int tris = 0;
    for (int f : out) tris += m.faces[f].n == 3 ? 1 : 0;
    CHECK(tris == 2);
}

void testBridgeHoles() {
    // Two boxes, facing sides torn open: select one edge of each hole.
    Mesh m = box({0, 0, 0}, {40, 40, 40});
    addBox(m, {80, 0, 0}, {120, 40, 40});
    int b = -1;
    for (int i = 0; i < (int)m.faces.size(); ++i)
        if (nearV(faceNormal(m, i), {-1, 0, 0}) && faceCentre(m, i).x == 80) b = i;
    MeshEdit e(m);
    e.deleteFaces({faceFacing(m, {1, 0, 0}), b});
    CHECK(!isClosed(m));
    std::string why;
    std::set<int> out;
    CHECK(bridgeHoles(e, {edgeAt(m, {40, 0, 0}, {40, 40, 0}), edgeAt(m, {80, 0, 40}, {80, 40, 40})}, {}, &out, &why));
    CHECK(isClosed(m) && good(m) && out.size() == 4 && near(signedVolume(m), 3 * 64000, 1e-6));
    // Only one hole touched: refused.
    Mesh n = box({0, 0, 0}, {40, 40, 40});
    MeshEdit en(n);
    en.deleteFaces({faceFacing(n, {0, 0, 1})});
    CHECK(!bridgeHoles(en, {edgeAt(n, {0, 0, 40}, {40, 0, 40})}, {}, nullptr, &why) && !why.empty());
}

Mesh plane(double size) {
    Mesh m;
    m.verts = {{0, 0, 0}, {size, 0, 0}, {size, size, 0}, {0, size, 0}};
    Face f;
    f.n = 4;
    for (int k = 0; k < 4; ++k) f.v[k] = k;
    m.faces = {f};
    return m;
}

void testShell() {
    std::string why;
    std::set<int> out;
    // A 100 square, 2 thick, inward (down): a closed 100x100x2 slab under it.
    for (auto side : {ShellSide::Inward, ShellSide::Outward, ShellSide::Centred}) {
        Mesh m = plane(100);
        MeshEdit e(m);
        ShellParams p;
        p.side = side;
        CHECK(shell(e, {0}, p, &out, &why));
        CHECK(isClosed(m) && good(m) && m.faces.size() == 6 && out.size() == 6);
        CHECK(near(signedVolume(m), 20000, 1e-6));
        Vec3 mn, mx;
        m.bounds(mn, mx);
        const double lo = side == ShellSide::Inward ? -2 : side == ShellSide::Outward ? 0 : -1;
        CHECK(mn.z == lo && mx.z == lo + 2);
    }
    // An open box (no top): walls 2 thick inward, even on the corners.
    Mesh b = box({0, 0, 0}, {100, 100, 100});
    MeshEdit eb(b);
    eb.deleteFaces({faceFacing(b, {0, 0, 1})});
    CHECK(shell(eb, {0}, {}, &out, &why));
    CHECK(isClosed(b) && good(b) && b.faces.size() == 14); // 5 walls, 5 inner, 4 rim
    CHECK(vertAt(b, {2, 2, 2}) >= 0 && vertAt(b, {98, 98, 100}) >= 0);
    CHECK(near(signedVolume(b), 1e6 - 96 * 96 * 98, 1e-6));
    // Closed: refused with a reason.
    Mesh c = box({0, 0, 0}, {10, 10, 10});
    MeshEdit ec(c);
    CHECK(!shell(ec, {0}, {}, nullptr, &why) && !why.empty());
    // Too thick for a sharp fold: refused (a V of two faces at a small angle).
    Mesh v;
    v.verts = {{0, 0, 0}, {100, 0, 0}, {100, 5, 100}, {0, 5, 100}, {100, 10, 0}, {0, 10, 0}};
    Face f1, f2;
    f1.n = f2.n = 4;
    int a1[4] = {0, 1, 2, 3}, a2[4] = {3, 2, 4, 5};
    for (int k = 0; k < 4; ++k) f1.v[k] = a1[k], f2.v[k] = a2[k];
    v.faces = {f1, f2};
    MeshEdit ev(v);
    CHECK(!shell(ev, {0}, {}, nullptr, &why));
}

void testRandomBevel() {
    // Random boxes, subdivided or not, random edges bevelled at random widths: either
    // refused with a reason (and the caller reverts), or a closed, clean model.
    std::mt19937 rng(7);
    int done = 0, refused = 0;
    for (int round = 0; round < 200; ++round) {
        Mesh m = box({0, 0, 0}, {60.0 + rng() % 60, 60.0 + rng() % 60, 60.0 + rng() % 60});
        MeshEdit e(m);
        if (rng() % 2) {
            std::set<int> all;
            for (int i = 0; i < 6; ++i) all.insert(i);
            subdivide(e, all, 2, 2);
        }
        std::vector<Edge> eds;
        for (const Face& f : m.faces)
            for (int k = 0; k < f.n; ++k) eds.push_back(makeEdge(f.v[k], f.v[(k + 1) % f.n]));
        std::set<Edge> pick;
        const int n = 1 + rng() % 5;
        for (int i = 0; i < n; ++i) pick.insert(eds[rng() % eds.size()]);
        BevelParams p;
        p.width = 1 + rng() % 25;
        p.segments = 1 + rng() % 3;
        std::string why;
        const Mesh before = m;
        bool ok;
        if (rng() % 4 == 0) {
            std::set<int> vs;
            for (const Edge& ed : pick) vs.insert(ed.first);
            ok = bevelPoints(e, vs, p.width, nullptr, &why);
        } else {
            ok = bevelEdges(e, pick, p, nullptr, &why);
        }
        if (ok) {
            ++done;
            CHECK(isClosed(m) && good(m));
            CHECK(signedVolume(m) > 0 && signedVolume(m) < signedVolume(before) + 1e-6);
        } else {
            ++refused;
            CHECK(!why.empty());
        }
    }
    std::printf("bevel rounds: %d done, %d refused\n", done, refused);
    CHECK(done > 100);
}

} // namespace

void testBevelOps() {
    testBevelOneEdge();
    testBevelCorner();
    testBevelAll();
    testBevelFlatEnd();
    testBevelTooWide();
    testBevelPoints();
    testBridge();
    testBridgeUneven();
    testBridgeHoles();
    testShell();
    testRandomBevel();
}
