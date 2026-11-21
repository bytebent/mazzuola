// v0.2.5: clean-up commands (core/clean_ops). Closed models stay closed.
#include "check.hpp"
#include "core/clean_ops.hpp"
#include "core/divide_ops.hpp"
#include "core/model_ops.hpp"
#include "core/ops.hpp"
#include "core/primitives.hpp"
#include <cmath>
#include <cstdio>

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
    for (const Face& f : m.faces) {
        if (f.n != 3 && f.n != 4) return false;
        for (int a = 0; a < f.n; ++a)
            for (int b = a + 1; b < f.n; ++b)
                if (f.v[a] == f.v[b]) return false;
    }
    return true;
}

void testMerge() {
    // The top's four corners to its centre: a pyramid.
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    std::set<int> top;
    for (int i = 0; i < (int)m.verts.size(); ++i)
        if (m.verts[i].z == 100) top.insert(i);
    MeshEdit e(m);
    CHECK(mergePoints(e, top, {50, 50, 100}));
    CHECK(m.verts.size() == 5 && m.faces.size() == 5);
    CHECK(isClosed(m) && good(m) && near(signedVolume(m), 1e6 / 3, 1e-3));
    // Two points at the first one's place.
    Mesh n = box({0, 0, 0}, {100, 100, 100});
    MeshEdit en(n);
    const int a = vertAt(n, {0, 0, 100}), b = vertAt(n, {100, 0, 100});
    CHECK(mergePoints(en, {a, b}, n.verts[b], b));
    CHECK(isClosed(n) && good(n) && n.verts.size() == 7);
    CHECK(vertAt(n, {100, 0, 100}) >= 0 && vertAt(n, {0, 0, 100}) < 0);
}

void testFirstSelected() {
    // Merge at first selected point: the document remembers which point came first.
    Document d;
    Selection s;
    s.mode = SelectMode::Vert;
    s.verts = {5};
    d.select(s);
    s.verts = {2, 5};
    d.select(s);
    CHECK(d.firstVert() == 5);
    s.verts = {2, 7};
    d.select(s);
    CHECK(d.firstVert() == 7 || d.firstVert() == 2);
    s.mode = SelectMode::Face;
    d.select(s);
    CHECK(d.firstVert() == -1);
}

void testCollapseAndWeld() {
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    MeshEdit e(m);
    CHECK(collapseEdges(e, {makeEdge(vertAt(m, {0, 0, 100}), vertAt(m, {100, 0, 100}))}));
    CHECK(isClosed(m) && good(m) && m.verts.size() == 7);
    CHECK(vertAt(m, {50, 0, 100}) >= 0);
    // Weld: a box mirror-cloned against itself shares a seam of points to weld.
    Mesh w = box({0, 0, 0}, {50, 100, 100});
    addBox(w, {50, 0, 0}, {100, 100, 100});
    MeshEdit ew(w);
    CHECK(weldByDistance(ew, {}, 0.1) == 4);
    CHECK(w.verts.size() == 12);
    // The two touching faces cancel out: one closed box with the seam inside gone.
    CHECK(isClosed(w) && good(w) && near(signedVolume(w), 1e6, 1e-3));
    Mesh far = box({0, 0, 0}, {10, 10, 10});
    MeshEdit ef(far);
    CHECK(weldByDistance(ef, {}, 0.1) == 0);
}

void testDissolve() {
    // A loop cut dissolved: back to a plain box.
    Mesh m = box({0, 0, 0}, {100, 60, 80});
    MeshEdit e(m);
    std::set<Edge> cut;
    CHECK(loopCut(e, loopRing(m, makeEdge(vertAt(m, {0, 0, 0}), vertAt(m, {0, 0, 80}))), 1, 0.5, &cut));
    CHECK(m.faces.size() == 10);
    std::string why;
    CHECK(dissolveEdges(e, cut, &why));
    if (!why.empty()) std::printf("dissolve edges: %s\n", why.c_str());
    CHECK(m.faces.size() == 6 && m.verts.size() == 8 && isClosed(m) && good(m));
    // A point on a top split 2x2 (the middle point): the four quads join.
    Mesh s = box({0, 0, 0}, {100, 100, 100});
    MeshEdit es(s);
    CHECK(subdivide(es, {faceFacing(s, {0, 0, 1})}, 2, 2));
    const int mid = vertAt(s, {50, 50, 100});
    CHECK(dissolvePoints(es, {mid}, &why));
    CHECK(vertAt(s, {50, 50, 100}) < 0 && isClosed(s) && good(s));
    CHECK(near(signedVolume(s), 1e6, 1e-3));
    // Faces: the two halves of a split top join again.
    Mesh f = box({0, 0, 0}, {100, 100, 100});
    MeshEdit ef(f);
    std::set<int> halves;
    CHECK(subdivide(ef, {faceFacing(f, {0, 0, 1})}, 2, 1, &halves));
    std::set<int> out;
    CHECK(dissolveFaces(ef, halves, &out, &why));
    CHECK(isClosed(f) && good(f));
    // One face alone shrinks to a point (a box's top: a pyramid).
    Mesh p = box({0, 0, 0}, {100, 100, 100});
    MeshEdit ep(p);
    CHECK(dissolveFaces(ep, {faceFacing(p, {0, 0, 1})}, nullptr, &why));
    CHECK(p.faces.size() == 5 && isClosed(p) && good(p));
}

void testFillDetachFlip() {
    // Tear open the top, then fill it: closed again, facing out.
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    MeshEdit e(m);
    e.deleteFaces({faceFacing(m, {0, 0, 1})});
    CHECK(!isClosed(m));
    std::set<int> top;
    for (int i = 0; i < (int)m.verts.size(); ++i)
        if (m.verts[i].z == 100) top.insert(i);
    int made = -1;
    std::string why;
    CHECK(fillFace(e, top, &made, &why));
    CHECK(isClosed(m) && near(signedVolume(m), 1e6, 1e-6));
    CHECK(nearV(faceNormal(m, made), {0, 0, 1}, 1e-9));
    CHECK(!fillFace(e, top, nullptr, &why)); // already there
    // Detach the top: it has its own points; the rest is open.
    Mesh d = box({0, 0, 0}, {100, 100, 100});
    MeshEdit ed(d);
    CHECK(detachFaces(ed, {faceFacing(d, {0, 0, 1})}));
    CHECK(d.verts.size() == 12 && !isClosed(d));
    // Flip: volume turns negative.
    Mesh f = box({0, 0, 0}, {10, 10, 10});
    MeshEdit ef(f);
    std::set<int> all;
    for (int i = 0; i < 6; ++i) all.insert(i);
    CHECK(flipFaces(ef, all));
    CHECK(near(signedVolume(f), -1000, 1e-9) && isClosed(f));
    std::vector<bool> before;
    for (const Face& q : f.faces) before.push_back(q.diag);
    CHECK(flipDiagonals(ef, all));
    for (size_t i = 0; i < f.faces.size(); ++i) CHECK(f.faces[i].diag != before[i]);
    // Triangulate and join back.
    Mesh t = box({0, 0, 0}, {10, 10, 10});
    MeshEdit et(t);
    std::set<int> tris;
    CHECK(triangulate(et, all, &tris));
    CHECK(t.faces.size() == 12 && isClosed(t));
    CHECK(joinIntoQuads(et, tris) == 6);
    CHECK(t.faces.size() == 6 && isClosed(t) && good(t) && near(signedVolume(t), 1000, 1e-9));
    // Double-sided.
    CHECK(setTwoSided(et, {0, 1}, true) && t.faces[0].twoSided && !t.faces[2].twoSided);
}

void testDissolveJoins() {
    // v0.2.5.1. Joined faces really become one: the points left in a straight line on
    // their outline go, and the faces beyond them get their old shape back.
    std::string why;
    // The two halves of a split top: the plain box again.
    Mesh h = box({0, 0, 0}, {100, 100, 100});
    MeshEdit eh(h);
    std::set<int> halves;
    CHECK(subdivide(eh, {faceFacing(h, {0, 0, 1})}, 2, 1, &halves));
    CHECK(h.faces.size() > 7);
    std::set<int> out;
    CHECK(dissolveFaces(eh, halves, &out, &why));
    CHECK(h.faces.size() == 6 && h.verts.size() == 8 && isClosed(h) && good(h));
    CHECK(near(signedVolume(h), 1e6, 1e-3));
    CHECK(out.size() == 1 && out.count(faceFacing(h, {0, 0, 1})));
    // A top split 2x2 (a middle point, four edge points): the plain box again.
    Mesh q = box({0, 0, 0}, {100, 100, 100});
    MeshEdit eq(q);
    std::set<int> quarters;
    CHECK(subdivide(eq, {faceFacing(q, {0, 0, 1})}, 2, 2, &quarters));
    CHECK(dissolveFaces(eq, quarters, &out, &why));
    CHECK(q.faces.size() == 6 && q.verts.size() == 8 && isClosed(q) && good(q));
    CHECK(near(signedVolume(q), 1e6, 1e-3));
    // Every side split 2x2; a 2x2 block on one side and one face alone on the opposite
    // side, together: the block joins, the lone face shrinks to its own centre (its
    // points must be followed while others are renumbered).
    const Vec3 dirs[6] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    for (const Vec3& d : dirs) {
        Mesh m = box({0, 0, 0}, {100, 100, 100});
        MeshEdit e(m);
        std::set<int> all;
        for (int i = 0; i < 6; ++i) all.insert(i);
        CHECK(subdivide(e, all, 2, 2));
        std::set<int> sel;
        int lone = -1;
        for (int i = 0; i < (int)m.faces.size(); ++i) {
            const Vec3 n = faceNormal(m, i);
            if (nearV(n, d, 1e-9)) sel.insert(i);
            else if (lone < 0 && nearV(n, d * -1.0, 1e-9)) lone = i;
        }
        CHECK(sel.size() == 4 && lone >= 0);
        const Vec3 centre = faceCentre(m, lone);
        sel.insert(lone);
        CHECK(dissolveFaces(e, sel, &out, &why));
        CHECK(vertAt(m, centre) >= 0 && isClosed(m) && good(m));
    }
}

void testDissolveTwins() {
    // Two points at one spot (after Detach): the selected one dissolves, not its twin.
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    MeshEdit e(m);
    const int top = faceFacing(m, {0, 0, 1});
    CHECK(detachFaces(e, {top}));
    const int copy = m.faces[top].v[0];
    int twin = -1;
    for (int i = 0; i < (int)m.verts.size(); ++i)
        if (i != copy && nearV(m.verts[i], m.verts[copy], 1e-9)) twin = i;
    CHECK(twin >= 0);
    std::string why;
    CHECK(dissolvePoints(e, {copy}, &why));
    const int topAfter = faceFacing(m, {0, 0, 1});
    CHECK(topAfter >= 0 && m.faces[topAfter].n == 3);
    int quads = 0;
    for (const Face& f : m.faces) quads += f.n == 4 ? 1 : 0;
    CHECK(quads == 5 && m.verts.size() == 11);
}

void testRandomClean() {
    // Random clean-ups on cut-up shapes: closed or refused.
    unsigned seed = 4711;
    auto rnd = [&](int lo, int hi) {
        seed = seed * 1103515245u + 12345u;
        return lo + (int)((seed >> 8) % (unsigned)(hi - lo + 1));
    };
    int bad = 0, done = 0;
    for (PrimKind kind : {PrimKind::Box, PrimKind::Cylinder, PrimKind::Sphere}) {
        PrimParams p;
        p.kind = kind;
        Mesh m = makePrimitive(p, {0, 0, 0}, {160, 120, 100}, 2);
        for (int round = 0; round < 80 && !bad; ++round) {
            MeshEdit e(m);
            const int fi = rnd(0, (int)m.faces.size() - 1);
            const Face f = m.faces[fi];
            const int k = rnd(0, f.n - 1);
            const Edge ed = makeEdge(f.v[k], f.v[(k + 1) % f.n]);
            bool ok = false;
            const int op = rnd(0, 7);
            switch (op) {
            case 0: ok = loopCut(e, loopRing(m, ed), 1, 0.5); break;
            case 1: ok = subdivide(e, {fi}, 2, 2); break;
            case 2: ok = dissolveEdges(e, {ed}); break;
            case 3: ok = dissolvePoints(e, {f.v[k]}); break;
            case 4: ok = collapseEdges(e, {ed}); break;
            case 5: ok = dissolveFaces(e, {fi}); break;
            case 7: {
                // A face and the faces sharing an edge with it.
                std::set<int> fs = {fi};
                for (int g = 0; g < (int)m.faces.size(); ++g)
                    for (int a = 0; a < m.faces[g].n; ++a)
                        if (m.faces[g].v[a] == f.v[(k + 1) % f.n] && m.faces[g].v[(a + 1) % m.faces[g].n] == f.v[k]) fs.insert(g);
                ok = dissolveFaces(e, fs);
                break;
            }
            default: ok = triangulate(e, {fi}) && joinIntoQuads(e, {fi}) >= 0; break;
            }
            if (!ok) {
                e.revert();
                continue;
            }
            ++done;
            if (!isClosed(m) || !good(m)) {
                ++bad;
                std::printf("clean round %d kind %d op %d closed %d good %d\n", round, (int)kind, op, isClosed(m), good(m));
            }
            if (m.faces.size() < 8) break;
        }
    }
    CHECK(bad == 0);
    std::printf("clean rounds done %d\n", done);
    CHECK(done > 60);
}

} // namespace

void testCleanOps() {
    testMerge();
    testFirstSelected();
    testCollapseAndWeld();
    testDissolve();
    testDissolveJoins();
    testDissolveTwins();
    testFillDetachFlip();
    testRandomClean();
}
