// v0.2.4: knife, loop cut, subdivide, divide edges, poke, connect, slide (core/divide_ops).
// Every result: a closed box stays closed with the same volume and good faces.
#include "check.hpp"
#include "core/divide_ops.hpp"
#include "core/model_ops.hpp"
#include "core/ops.hpp"
#include "core/primitives.hpp"
#include <cmath>
#include <cstdio>
#include <map>

using namespace mazz;

namespace {

Mesh box(const Vec3& a, const Vec3& b) {
    Mesh m;
    addBox(m, a, b);
    return m;
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
        Vec3 s{0, 0, 0};
        for (int k = 1; k + 1 < f.n; ++k) s = s + cross(m.verts[f.v[k]] - m.verts[f.v[0]], m.verts[f.v[k + 1]] - m.verts[f.v[0]]);
        if (length(s) / 2 < 1e-3) return false;
    }
    return true;
}

int vertAt(const Mesh& m, const Vec3& p) {
    for (int i = 0; i < (int)m.verts.size(); ++i)
        if (nearV(m.verts[i], p, 1e-6)) return i;
    return -1;
}

void testKnife() {
    // Round a box's corner: front edge -> top edge -> side edge -> front edge.
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    MeshEdit e(m);
    std::string why;
    CHECK(knife(e, {{80, 0, 100}, {100, 0, 80}, {100, 20, 100}, {80, 0, 100}}, &why));
    CHECK(why.empty());
    CHECK(isClosed(m) && good(m) && near(signedVolume(m), 1e6, 1e-3));
    CHECK(m.faces.size() > 6);
    // Two points not on one face: refused.
    Mesh r = box({0, 0, 0}, {100, 100, 100});
    MeshEdit er(r);
    CHECK(!knife(er, {{50, 0, 0}, {50, 100, 100}}, &why));
}

void testLoopCut() {
    // A box: the ring through an upright edge goes round the four sides.
    Mesh m = box({0, 0, 0}, {100, 60, 80});
    const int a = vertAt(m, {0, 0, 0}), b = vertAt(m, {0, 0, 80});
    LoopRing r = loopRing(m, makeEdge(a, b));
    CHECK(r.valid() && r.closed && r.faces.size() == 4);
    CHECK(loopCutLines(m, r, 1, 0.5).size() == 4);
    MeshEdit e(m);
    std::set<Edge> cut;
    CHECK(loopCut(e, r, 1, 0.5, &cut));
    CHECK(cut.size() == 4 && m.faces.size() == 10);
    CHECK(isClosed(m) && good(m) && near(signedVolume(m), 100 * 60 * 80, 1e-6));
    for (const Edge& ed : cut) CHECK(near(m.verts[ed.first].z, 40, 1e-9) && near(m.verts[ed.second].z, 40, 1e-9));
    // Three cuts, off-centre position ignored (evenly spread).
    Mesh t = box({0, 0, 0}, {100, 60, 80});
    MeshEdit et(t);
    LoopRing rt = loopRing(t, makeEdge(vertAt(t, {0, 0, 0}), vertAt(t, {0, 0, 80})));
    CHECK(loopCut(et, rt, 3, 0.2));
    CHECK(t.faces.size() == 2 + 16 && isClosed(t) && good(t));
    // One cut at 25%.
    Mesh q = box({0, 0, 0}, {100, 60, 80});
    MeshEdit eq(q);
    std::set<Edge> c25;
    LoopRing rq = loopRing(q, makeEdge(vertAt(q, {0, 0, 0}), vertAt(q, {0, 0, 80})));
    CHECK(loopCut(eq, rq, 1, 0.25, &c25));
    const double z = q.verts[c25.begin()->first].z;
    CHECK(near(z, 20, 1e-9) || near(z, 60, 1e-9));
    for (const Edge& ed : c25) CHECK(near(q.verts[ed.first].z, z, 1e-9));
    // An open ring that ends at triangles: the triangles get the points, still closed.
    PrimParams p;
    p.kind = PrimKind::Cylinder;
    Mesh c = makePrimitive(p, {0, 0, 0}, {100, 100, 100}, 2);
    int up = -1;
    for (const Face& f : c.faces)
        if (f.n == 4)
            for (int k = 0; k < 4 && up < 0; ++k) {
                const Vec3 d = c.verts[f.v[(k + 1) % 4]] - c.verts[f.v[k]];
                if (std::fabs(d.x) < 1e-9 && std::fabs(d.y) < 1e-9) up = k, up = (int)makeEdge(f.v[k], f.v[(k + 1) % 4]).first;
            }
    // A horizontal edge of a side quad: the ring goes up the side, over the caps if quads.
    Mesh c2 = c;
    for (const Face& f : c2.faces) {
        if (f.n != 4) continue;
        for (int k = 0; k < 4; ++k) {
            const int u = f.v[k], w = f.v[(k + 1) % 4];
            if (std::fabs(c2.verts[u].z - c2.verts[w].z) < 1e-9 && std::fabs(c2.verts[u].z) < 1e-9) {
                LoopRing rr = loopRing(c2, makeEdge(u, w));
                MeshEdit e2(c2);
                CHECK(rr.valid());
                const double v0 = signedVolume(c2);
                CHECK(loopCut(e2, rr, 2, 0.5));
                CHECK(isClosed(c2) && good(c2) && near(signedVolume(c2), v0, 1e-3));
                goto done;
            }
        }
    }
done:
    (void)up;
}

void testSubdivide() {
    Mesh m = box({0, 0, 0}, {120, 60, 80});
    const int top = faceFacing(m, {0, 0, 1});
    MeshEdit e(m);
    std::set<int> out;
    CHECK(subdivide(e, {top}, 3, 2, &out));
    CHECK(out.size() == 6);
    CHECK(isClosed(m) && good(m) && near(signedVolume(m), 120 * 60 * 80, 1e-6));
    // The whole box 2x2: 24 faces, closed.
    Mesh b = box({0, 0, 0}, {100, 100, 100});
    MeshEdit eb(b);
    std::set<int> all;
    for (int i = 0; i < (int)b.faces.size(); ++i) all.insert(i);
    CHECK(subdivide(eb, all, 2, 2, &out));
    CHECK(b.faces.size() == 24 && isClosed(b) && good(b));
    // Different numbers each way on neighbours: refused.
    Mesh d = box({0, 0, 0}, {100, 100, 100});
    MeshEdit ed(d);
    std::string why;
    all.clear();
    for (int i = 0; i < (int)d.faces.size(); ++i) all.insert(i);
    CHECK(!subdivide(ed, all, 3, 2, nullptr, &why) && !why.empty());
    // A triangle 3x3: 9 triangles.
    Mesh t;
    t.verts = {{0, 0, 0}, {90, 0, 0}, {0, 90, 0}};
    Face f;
    f.n = 3;
    f.v[0] = 0, f.v[1] = 1, f.v[2] = 2;
    t.faces.push_back(f);
    MeshEdit et(t);
    CHECK(subdivide(et, {0}, 3, 3, &out));
    CHECK(t.faces.size() == 9 && good(t));
}

void testDivideEdges() {
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    MeshEdit e(m);
    const int a = vertAt(m, {0, 0, 100}), b = vertAt(m, {100, 0, 100});
    CHECK(divideEdges(e, {makeEdge(a, b)}, 4));
    CHECK(m.verts.size() == 11 && isClosed(m) && good(m) && near(signedVolume(m), 1e6, 1e-6));
}

void testPoke() {
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    const int top = faceFacing(m, {0, 0, 1});
    MeshEdit e(m);
    std::set<int> out;
    CHECK(poke(e, {top}, 30, &out));
    CHECK(out.size() == 4 && m.faces.size() == 9);
    CHECK(isClosed(m) && good(m) && near(signedVolume(m), 1e6 + 100 * 100 * 30 / 3.0, 1e-3));
}

void testConnect() {
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    MeshEdit e(m);
    const int a = vertAt(m, {0, 0, 100}), b = vertAt(m, {100, 100, 100});
    std::string why;
    CHECK(connectPoints(e, a, b, &why));
    CHECK(m.faces.size() == 7 && isClosed(m) && good(m));
    CHECK(!connectPoints(e, a, vertAt(m, {100, 0, 100}), &why)); // already an edge
    CHECK(!connectPoints(e, vertAt(m, {0, 0, 0}), b, &why));    // not on one face
}

void testSlideTarget() {
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    const int v = vertAt(m, {0, 0, 0});
    CHECK(slideTarget(m, v, {1, 0.1, 0}) == vertAt(m, {100, 0, 0}));
    CHECK(slideTarget(m, v, {0, 0, 1}) == vertAt(m, {0, 0, 100}));
}

void testRandomDivides() {
    // Random loop cuts, subdivides, divides, pokes and knife steps, one after another on
    // a box, a cylinder and a sphere: always closed with good faces (or refused).
    unsigned seed = 555;
    auto rnd = [&](int lo, int hi) {
        seed = seed * 1103515245u + 12345u;
        return lo + (int)((seed >> 8) % (unsigned)(hi - lo + 1));
    };
    int bad = 0, done = 0;
    for (PrimKind kind : {PrimKind::Box, PrimKind::Cylinder, PrimKind::Sphere}) {
        PrimParams p;
        p.kind = kind;
        Mesh m = makePrimitive(p, {0, 0, 0}, {160, 120, 100}, 2);
        for (int round = 0; round < 60 && !bad; ++round) {
            const Mesh before = m;
            MeshEdit e(m);
            const int fi = rnd(0, (int)m.faces.size() - 1);
            const Face f = m.faces[fi];
            const int k = rnd(0, f.n - 1);
            bool ok = false;
            const int op = rnd(0, 4);
            switch (op) {
            case 0: ok = loopCut(e, loopRing(m, makeEdge(f.v[k], f.v[(k + 1) % f.n])), rnd(1, 3), rnd(10, 90) / 100.0); break;
            case 1: ok = subdivide(e, {fi}, 2, 2); break;
            case 2: ok = divideEdges(e, {makeEdge(f.v[k], f.v[(k + 1) % f.n])}, rnd(2, 4)); break;
            case 3: ok = poke(e, {fi}, rnd(-10, 20)); break;
            default: {
                const Vec3 a = (m.verts[f.v[k]] + m.verts[f.v[(k + 1) % f.n]]) * 0.5;
                const int k2 = (k + 1 + rnd(0, f.n - 2)) % f.n;
                const Vec3 b = (m.verts[f.v[k2]] + m.verts[f.v[(k2 + 1) % f.n]]) * 0.5;
                ok = k2 != k && knife(e, {a, b});
            }
            }
            if (!ok) {
                e.revert();
                continue;
            }
            ++done;
            if (!isClosed(m) || !good(m)) {
                ++bad;
                std::printf("divide round %d kind %d op %d closed %d good %d\n", round, (int)kind, op, isClosed(m), good(m));
            }
        }
    }
    CHECK(bad == 0);
    CHECK(done > 60);
}

} // namespace

void testDivideOps() {
    testRandomDivides();
    testKnife();
    testLoopCut();
    testSubdivide();
    testDivideEdges();
    testPoke();
    testConnect();
    testSlideTarget();
}
