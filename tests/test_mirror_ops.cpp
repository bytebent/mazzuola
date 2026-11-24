// v0.2.7: mirror mode (core/mirror_ops): twins, the fix-up, Make symmetric.
#include "check.hpp"
#include "core/divide_ops.hpp"
#include "core/mirror_ops.hpp"
#include "core/model_ops.hpp"
#include "core/ops.hpp"
#include "core/primitives.hpp"
#include <cmath>

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

void testTwins() {
    Mesh m = box({-50, 0, 0}, {50, 40, 40});
    const auto t = twinPoints(m, 0);
    for (int i = 0; i < 8; ++i) {
        CHECK(t[i] >= 0 && t[i] != i);
        CHECK(nearV(m.verts[t[i]], mirrored(m.verts[i], 0), 1e-9));
    }
    CHECK(unpairedPoints(m, 0) == 0);
    CHECK(unpairedPoints(m, 1) == 4); // across y = 0: the four at y = 0 are on the plane
    Mesh off = box({10, 0, 0}, {60, 40, 40});
    CHECK(unpairedPoints(off, 0) == 8);
    Mesh touching = box({0, 0, 0}, {60, 40, 40});
    CHECK(unpairedPoints(touching, 0) == 4); // the four at x = 0 are their own twins
    // Twin selection: a point, an edge, a face.
    Selection s;
    s.mode = SelectMode::Vert;
    s.verts = {vertAt(m, {50, 40, 40})};
    Selection w = withTwins(m, s, 0);
    CHECK(w.verts.size() == 2 && w.verts.count(vertAt(m, {-50, 40, 40})));
    s = Selection();
    s.mode = SelectMode::Edge;
    s.edges = {makeEdge(vertAt(m, {50, 0, 40}), vertAt(m, {50, 40, 40}))};
    w = withTwins(m, s, 0);
    CHECK(w.edges.size() == 2 && w.edges.count(makeEdge(vertAt(m, {-50, 0, 40}), vertAt(m, {-50, 40, 40}))));
    s = Selection();
    s.mode = SelectMode::Face;
    s.faces = {faceFacing(m, {1, 0, 0})};
    w = withTwins(m, s, 0);
    CHECK(w.faces.size() == 2 && w.faces.count(faceFacing(m, {-1, 0, 0})));
    s.faces = {faceFacing(m, {0, 0, 1})}; // the top crosses the plane: its own twin
    CHECK(withTwins(m, s, 0).faces.size() == 1);
    CHECK(withTwins(m, s, -1) == s);
}

void testFix() {
    // Moving a corner on +X: its twin goes the mirrored way.
    Mesh m = box({-50, 0, 0}, {50, 40, 40});
    const auto t = twinPoints(m, 0);
    const int a = vertAt(m, {50, 40, 40}), b = vertAt(m, {-50, 40, 40});
    {
        MeshEdit e(m);
        e.setVert(a, {70, 60, 40});
        mirrorFix(e, 0, 1, t);
        CHECK(nearV(m.verts[b], {-70, 60, 40}, 1e-9));
        e.revert();
    }
    // Both moved the same way (a tool moving the twin selection by +10 x): mirrored.
    {
        MeshEdit e(m);
        e.setVert(a, {60, 40, 40});
        e.setVert(b, {-40, 40, 40});
        mirrorFix(e, 0, 1, t);
        CHECK(nearV(m.verts[b], {-60, 40, 40}, 1e-9));
        e.revert();
    }
    // Working on the -X side: +X follows.
    {
        MeshEdit e(m);
        e.setVert(b, {-80, 40, 40});
        mirrorFix(e, 0, -1, t);
        CHECK(nearV(m.verts[a], {80, 40, 40}, 1e-9));
        e.revert();
    }
    // A turn about Z (both sides turned the same way): the other side turns back.
    {
        Mesh r = box({-50, 0, 0}, {50, 40, 40});
        const auto tr = twinPoints(r, 0);
        MeshEdit e(r);
        Selection all;
        all.mode = SelectMode::Face;
        for (int i = 0; i < 6; ++i) all.faces.insert(i);
        TransformParams tp;
        tp.rotate = {0, 0, 30};
        tp.pivot = PivotKind::Origin;
        applyTransform(e, all, tp);
        mirrorFix(e, 0, 1, tr);
        for (int i = 0; i < 8; ++i) CHECK(nearV(r.verts[tr[i]], mirrored(r.verts[i], 0), 2e-3));
    }
    // A point on the plane stays on it; the move along the plane is kept.
    {
        Mesh p = box({-50, 0, 0}, {50, 40, 40});
        MeshEdit e(p);
        CHECK(subdivide(e, {faceFacing(p, {0, 0, 1})}, 2, 1) || subdivide(e, {faceFacing(p, {0, 0, 1})}, 1, 2));
        const int mid = vertAt(p, {0, 0, 40}) >= 0 ? vertAt(p, {0, 0, 40}) : vertAt(p, {0, 40, 40});
        CHECK(mid >= 0);
        const auto tp = twinPoints(p, 0);
        CHECK(tp[mid] == mid);
        MeshEdit e2(p);
        const Vec3 was = p.verts[mid];
        e2.setVert(mid, was + Vec3{7, 0, 5});
        mirrorFix(e2, 0, 1, tp);
        CHECK(nearV(p.verts[mid], was + Vec3{0, 0, 5}, 1e-9));
    }
    // A point without a twin moves alone.
    {
        Mesh u = box({10, 0, 0}, {60, 40, 40});
        const auto tu = twinPoints(u, 0);
        MeshEdit e(u);
        e.setVert(0, u.verts[0] + Vec3{5, 0, 0});
        const Mesh before = u;
        mirrorFix(e, 0, 1, tu);
        for (int i = 1; i < 8; ++i) CHECK(nearV(u.verts[i], before.verts[i], 0));
    }
}

void testMakeSymmetric() {
    std::string why;
    // A box off centre: keep +X, it becomes -50..50; keep -X, -30..30.
    for (int keep : {1, -1}) {
        Mesh m = box({-30, 0, 0}, {50, 40, 40});
        MeshEdit e(m);
        CHECK(makeSymmetric(e, 0, keep, &why));
        CHECK(isClosed(m) && unpairedPoints(m, 0) == 0);
        Vec3 mn, mx;
        m.bounds(mn, mx);
        const double h = keep > 0 ? 50 : 30;
        CHECK(near(mn.x, -h, 1e-9) && near(mx.x, h, 1e-9));
        CHECK(near(signedVolume(m), 2 * h * 1600, 1e-6));
        for (const Face& f : m.faces) CHECK(f.n == 3 || f.n == 4);
    }
    // Touching the plane with a face lying in it: that face goes, the halves join.
    Mesh t = box({0, 0, 0}, {50, 40, 40});
    MeshEdit et(t);
    CHECK(makeSymmetric(et, 0, 1, &why));
    CHECK(isClosed(t) && near(signedVolume(t), 100 * 1600, 1e-6) && t.verts.size() == 12 && t.faces.size() == 10);
    // Apart from the plane: a mirrored copy; the empty side: refused.
    Mesh a = box({10, 0, 0}, {60, 40, 40});
    MeshEdit ea(a);
    CHECK(makeSymmetric(ea, 0, 1, &why));
    CHECK(isClosed(a) && a.verts.size() == 16 && near(signedVolume(a), 2 * 50 * 1600, 1e-6));
    Mesh b = box({10, 0, 0}, {60, 40, 40});
    MeshEdit eb(b);
    CHECK(!makeSymmetric(eb, 0, -1, &why) && !why.empty());
    // A cylinder with an odd number of sides, off centre in Y: symmetric across Y.
    PrimParams cp;
    cp.kind = PrimKind::Cylinder;
    cp.sides = 7;
    Mesh c = makePrimitive(cp, {-20, -13, 0}, {20, 27, 50}, 2);
    MeshEdit ec(c);
    CHECK(makeSymmetric(ec, 1, 1, &why));
    CHECK(isClosed(c) && unpairedPoints(c, 1) == 0);
    for (const Face& f : c.faces) CHECK(f.n == 3 || f.n == 4);
}

} // namespace

void testMirrorOps() {
    testTwins();
    testFix();
    testMakeSymmetric();
}
