// Milestone 2, v0.2.0: push/pull, extrude, inset (core/model_ops). Every result is a
// valid mesh; a closed surface stays closed and faces outward (positive volume).
#include "check.hpp"
#include "core/model_ops.hpp"
#include "core/ops.hpp"
#include "core/primitives.hpp"
#include <cmath>

using namespace mazz;

namespace {

// The face of a box whose normal is `n` (box made by addBox: one face per side).
int faceFacing(const Mesh& m, const Vec3& n) {
    for (int i = 0; i < (int)m.faces.size(); ++i)
        if (nearV(faceNormal(m, i), n, 1e-9)) return i;
    return -1;
}

Mesh box(const Vec3& a, const Vec3& b) {
    Mesh m;
    addBox(m, a, b);
    return m;
}

Mesh quad(double w, double h) {
    Mesh m;
    m.verts = {{0, 0, 0}, {w, 0, 0}, {w, h, 0}, {0, h, 0}};
    Face f;
    f.n = 4;
    for (int k = 0; k < 4; ++k) f.v[k] = k;
    m.faces.push_back(f);
    return m;
}

// A box whose top is split into two quads (x 0..50 and 50..100).
Mesh boxSplitTop() {
    PrimParams p;
    p.kind = PrimKind::Box;
    p.segX = 2;
    return makePrimitive(p, {0, 0, 0}, {100, 100, 100}, 2);
}

bool validIndices(const Mesh& m) {
    for (const Face& f : m.faces) {
        if (f.n != 3 && f.n != 4) return false;
        for (int k = 0; k < f.n; ++k)
            if (f.v[k] < 0 || f.v[k] >= (int)m.verts.size()) return false;
    }
    return true;
}

void testPushStretch() {
    Mesh m = box({0, 0, 0}, {100, 50, 40});
    const int top = faceFacing(m, {0, 0, 1});
    CHECK(pushKind(m, top, false) == PushKind::Stretch);
    CHECK(pushKind(m, top, true) == PushKind::NewFaces);
    MeshEdit e(m);
    int moved = -1;
    CHECK(pushPull(e, top, 24, false, &moved));
    CHECK(moved == top);
    CHECK(m.verts.size() == 8 && m.faces.size() == 6);
    Vec3 mn, mx;
    m.bounds(mn, mx);
    CHECK(mx.z == 64 && mn.z == 0);
    CHECK(isClosed(m));
    CHECK(near(signedVolume(m), 100 * 50 * 64, 1e-6));
    // Pushing in past the bottom turns the sides over: refused.
    Mesh m2 = box({0, 0, 0}, {100, 50, 40});
    MeshEdit e2(m2);
    CHECK(!pushPull(e2, top, -40, false));
    e2.revert();
    MeshEdit e3(m2);
    CHECK(!pushPull(e3, top, -60, false));
    e3.revert();
    MeshEdit e4(m2);
    CHECK(pushPull(e4, top, -39, false));
    CHECK(isClosed(m2) && near(signedVolume(m2), 100 * 50 * 1, 1e-6));
}

void testPushNewFaces() {
    // Cmd: new faces even on a box.
    Mesh m = box({0, 0, 0}, {100, 50, 40});
    const int top = faceFacing(m, {0, 0, 1});
    MeshEdit e(m);
    int moved = -1;
    CHECK(pushPull(e, top, 10, true, &moved));
    CHECK(m.verts.size() == 12 && m.faces.size() == 10);
    CHECK(isClosed(m) && validIndices(m));
    CHECK(near(signedVolume(m), 100 * 50 * 50, 1e-6));
    CHECK(nearV(faceCentre(m, moved), {50, 25, 50}, 1e-9));
    // In: a recess (volume goes down, still closed and outward).
    Mesh r = box({0, 0, 0}, {100, 50, 40});
    MeshEdit er(r);
    CHECK(pushPull(er, top, -10, true));
    CHECK(isClosed(r) && near(signedVolume(r), 100 * 50 * 30, 1e-6));

    // A face whose neighbours aren't at right angles (top split in two): new faces.
    Mesh s = boxSplitTop();
    int half = -1;
    for (int i = 0; i < (int)s.faces.size(); ++i)
        if (nearV(faceNormal(s, i), {0, 0, 1}, 1e-9) && faceCentre(s, i).x < 50) half = i;
    CHECK(half >= 0);
    CHECK(pushKind(s, half, false) == PushKind::NewFaces);
    const double v0 = signedVolume(s);
    MeshEdit es(s);
    CHECK(pushPull(es, half, 20, false));
    CHECK(isClosed(s) && near(signedVolume(s), v0 + 50 * 100 * 20, 1e-6));
}

void testPushFlatShape() {
    // A lone rectangle pulled up becomes a closed box.
    Mesh m = quad(100, 60);
    CHECK(pushKind(m, 0, false) == PushKind::Solid);
    MeshEdit e(m);
    int moved = -1;
    CHECK(pushPull(e, 0, 30, false, &moved));
    CHECK(m.faces.size() == 6 && m.verts.size() == 8);
    CHECK(isClosed(m) && near(signedVolume(m), 100 * 60 * 30, 1e-6));
    CHECK(nearV(faceNormal(m, moved), {0, 0, 1}, 1e-9));
    // Pushed down (against its normal): a box below, still outward.
    Mesh d = quad(100, 60);
    MeshEdit ed(d);
    CHECK(pushPull(ed, 0, -30, false, &moved));
    CHECK(isClosed(d) && near(signedVolume(d), 100 * 60 * 30, 1e-6));
    Vec3 mn, mx;
    d.bounds(mn, mx);
    CHECK(mn.z == -30 && mx.z == 0);
    // Nothing to push.
    Mesh z = quad(10, 10);
    MeshEdit ez(z);
    CHECK(!pushPull(ez, 0, 0, false));
}

void testExtrudeRegion() {
    // Two top faces as one region: one wall all round, no wall between them.
    Mesh s = boxSplitTop();
    std::set<int> tops;
    for (int i = 0; i < (int)s.faces.size(); ++i)
        if (nearV(faceNormal(s, i), {0, 0, 1}, 1e-9)) tops.insert(i);
    CHECK(tops.size() == 2);
    const size_t faces0 = s.faces.size();
    const double v0 = signedVolume(s);
    MeshEdit e(s);
    ExtrudeParams p;
    p.distance = 10;
    std::set<int> out;
    CHECK(extrudeFaces(e, tops, p, &out));
    CHECK(out == tops);
    CHECK(s.faces.size() == faces0 + 6); // 6 boundary edges
    CHECK(isClosed(s) && near(signedVolume(s), v0 + 100 * 100 * 10, 1e-6));
    // Each face on its own: a wall between them too.
    Mesh t = boxSplitTop();
    MeshEdit et(t);
    p.each = true;
    CHECK(extrudeFaces(et, tops, p, &out));
    CHECK(t.faces.size() == faces0 + 8);
    CHECK(near(signedVolume(t), v0 + 100 * 100 * 10, 1e-6));
}

void testExtrudeCorner() {
    // Top and front of a box together, along normals: both move out by exactly 10.
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    const int top = faceFacing(m, {0, 0, 1}), front = faceFacing(m, {0, -1, 0});
    MeshEdit e(m);
    ExtrudeParams p;
    p.distance = 10;
    std::set<int> out;
    CHECK(extrudeFaces(e, {top, front}, p, &out));
    CHECK(isClosed(m));
    for (int f : out) {
        const Vec3 c = faceCentre(m, f), n = faceNormal(m, f);
        if (n.z > 0.5) CHECK(near(c.z, 110, 1e-9));
        else CHECK(near(c.y, -10, 1e-9));
    }
}

void testExtrudeStraightKeep() {
    // A flat shape straight up with Keep original: a closed solid.
    Mesh m = quad(40, 20);
    MeshEdit e(m);
    ExtrudeParams p;
    p.straight = true;
    p.axis = 2;
    p.distance = 15;
    p.keepOriginal = true;
    std::set<int> out;
    CHECK(extrudeFaces(e, {0}, p, &out));
    CHECK(isClosed(m) && near(signedVolume(m), 40 * 20 * 15, 1e-6));
    CHECK(out.size() == 1 && near(faceCentre(m, *out.begin()).z, 15, 1e-12));
    // Straight down with Keep original: also outward.
    Mesh d = quad(40, 20);
    MeshEdit ed(d);
    p.distance = -15;
    CHECK(extrudeFaces(ed, {0}, p, &out));
    CHECK(isClosed(d) && near(signedVolume(d), 40 * 20 * 15, 1e-6));
    // Straight sideways (in the face's own plane): nothing sensible, refused.
    Mesh s = quad(40, 20);
    MeshEdit es(s);
    p.axis = 0;
    p.distance = 10;
    CHECK(!extrudeFaces(es, {0}, p));
}

void testExtrudeEdges() {
    // The far edge of a flat quad extruded up: a second face, joined the right way.
    Mesh m = quad(40, 20);
    MeshEdit e(m);
    std::set<Edge> out;
    CHECK(openEdges(m, {makeEdge(2, 3)}).size() == 1);
    CHECK(extrudeEdges(e, {makeEdge(2, 3)}, {0, 0, 10}, &out));
    CHECK(m.faces.size() == 2 && m.verts.size() == 6 && out.size() == 1);
    // The shared edge runs opposite ways in the two faces (consistent turning).
    std::set<std::pair<int, int>> dirs;
    bool clash = false;
    for (const Face& f : m.faces)
        for (int k = 0; k < f.n; ++k)
            if (!dirs.insert({f.v[k], f.v[(k + 1) % f.n]}).second) clash = true;
    CHECK(!clash);
    // Closed edges (box) can't be extruded.
    Mesh b = box({0, 0, 0}, {10, 10, 10});
    MeshEdit eb(b);
    CHECK(!extrudeEdges(eb, {makeEdge(b.faces[0].v[0], b.faces[0].v[1])}, {0, 0, 5}));
}

void testInset() {
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    const int top = faceFacing(m, {0, 0, 1});
    MeshEdit e(m);
    InsetParams p;
    p.width = 10;
    std::set<int> out;
    CHECK(inset(e, {top}, p, &out));
    CHECK(out == std::set<int>{top});
    CHECK(m.faces.size() == 10 && m.verts.size() == 12);
    CHECK(isClosed(m) && near(signedVolume(m), 1e6, 1e-6));
    Vec3 mn{1e9, 1e9, 1e9}, mx{-1e9, -1e9, -1e9};
    for (int k = 0; k < 4; ++k) {
        const Vec3 v = m.verts[m.faces[top].v[k]];
        mn = {std::min(mn.x, v.x), std::min(mn.y, v.y), v.z};
        mx = {std::max(mx.x, v.x), std::max(mx.y, v.y), v.z};
    }
    CHECK(mn.x == 10 && mn.y == 10 && mx.x == 90 && mx.y == 90 && mn.z == 100);
    // With depth: a recessed panel.
    Mesh d = box({0, 0, 0}, {100, 100, 100});
    MeshEdit ed(d);
    p.depth = 5;
    CHECK(inset(ed, {top}, p, &out));
    CHECK(isClosed(d) && near(signedVolume(d), 1e6 - 80 * 80 * 5, 1e-6));
    // Too wide: refused.
    Mesh w = box({0, 0, 0}, {100, 100, 100});
    MeshEdit ew(w);
    p.depth = 0;
    p.width = 50;
    CHECK(!inset(ew, {top}, p));
    ew.revert();
    MeshEdit ew2(w);
    p.width = 49;
    CHECK(inset(ew2, {top}, p));
    // A triangle.
    Mesh t;
    t.verts = {{0, 0, 0}, {100, 0, 0}, {0, 100, 0}};
    Face f;
    f.n = 3;
    f.v[0] = 0, f.v[1] = 1, f.v[2] = 2;
    t.faces.push_back(f);
    MeshEdit et(t);
    p.width = 10;
    CHECK(inset(et, {0}, p));
    CHECK(t.faces.size() == 4 && validIndices(t));
    CHECK(nearV(t.verts[t.faces[0].v[0]], {10, 10, 0}, 1e-9));
}

void testInsetRegion() {
    Mesh s = boxSplitTop();
    std::set<int> tops;
    for (int i = 0; i < (int)s.faces.size(); ++i)
        if (nearV(faceNormal(s, i), {0, 0, 1}, 1e-9)) tops.insert(i);
    const size_t faces0 = s.faces.size();
    MeshEdit e(s);
    InsetParams p;
    p.width = 10;
    CHECK(inset(e, tops, p));
    CHECK(s.faces.size() == faces0 + 6);
    CHECK(isClosed(s));
    // Each: a ring round each face.
    Mesh t = boxSplitTop();
    MeshEdit et(t);
    p.each = true;
    CHECK(inset(et, tops, p));
    CHECK(t.faces.size() == faces0 + 8);
    CHECK(isClosed(t));
}

void testFlippedKeepsDiagonal() {
    // Turning a quad round keeps the same triangles (the split stays on the same points).
    Face f;
    f.n = 4;
    for (int k = 0; k < 4; ++k) f.v[k] = 10 + k;
    const Face g = flipped(f);
    // f splits 10-12; g = 13,12,11,10 must split 12-10 too: v1-v3 -> diag.
    CHECK(g.diag);
    CHECK(!flipped(g).diag);
}

} // namespace

void testModelOps() {
    testPushStretch();
    testPushNewFaces();
    testPushFlatShape();
    testExtrudeRegion();
    testExtrudeCorner();
    testExtrudeStraightKeep();
    testExtrudeEdges();
    testInset();
    testInsetRegion();
    testFlippedKeepsDiagonal();
}
