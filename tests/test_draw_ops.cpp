// v0.2.2: drawing on faces (core/draw_ops). Every result: faces of 3 or 4 corners with
// area, a closed box stays closed with the same volume, the face's area is kept.
#include "check.hpp"
#include "core/draw_ops.hpp"
#include "core/model_ops.hpp"
#include "core/numbers.hpp"
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

double faceArea(const Mesh& m, int fi) {
    const Face& f = m.faces[fi];
    Vec3 s{0, 0, 0};
    for (int k = 1; k + 1 < f.n; ++k)
        s = s + cross(m.verts[f.v[k]] - m.verts[f.v[0]], m.verts[f.v[k + 1]] - m.verts[f.v[0]]);
    return length(s) / 2;
}

double areaFacing(const Mesh& m, const Vec3& n) {
    double a = 0;
    for (int i = 0; i < (int)m.faces.size(); ++i)
        if (nearV(faceNormal(m, i), n, 1e-6)) a += faceArea(m, i);
    return a;
}

// Faces of 3-4 corners, no repeated corner, some area, flat.
bool goodFaces(const Mesh& m, bool tell = false) {
    for (int i = 0; i < (int)m.faces.size(); ++i) {
        const Face& f = m.faces[i];
        if (f.n != 3 && f.n != 4) return false;
        for (int a = 0; a < f.n; ++a)
            for (int b = a + 1; b < f.n; ++b)
                if (f.v[a] == f.v[b]) return false;
        if (faceArea(m, i) < 1e-3) {
            if (tell) std::printf("face %d area %g\n", i, faceArea(m, i));
            return false;
        }
        // Every corner turns the same way (convex, no straight corners in quads).
        const Vec3 n = faceNormal(m, i);
        for (int k = 0; k < f.n; ++k) {
            const Vec3 a = m.verts[f.v[(k + f.n - 1) % f.n]], b = m.verts[f.v[k]], c = m.verts[f.v[(k + 1) % f.n]];
            if (dot(cross(b - a, c - b), n) <= 1e-9) {
                if (tell) std::printf("face %d (%d corners) corner %d turn %g\n", i, f.n, k, dot(cross(b - a, c - b), n));
                return false;
            }
        }
    }
    return true;
}

void testSplitPolygon() {
    Mesh m;
    m.verts = {{0, 0, 0}, {100, 0, 0}, {100, 100, 0}, {0, 100, 0}};
    CHECK(splitPolygon(m, {0, 1, 2, 3}, {0, 0, 1}).size() == 1);
    // A point on an edge: no face may have a straight corner.
    m.verts.push_back({50, 0, 0});
    auto s = splitPolygon(m, {0, 4, 1, 2, 3}, {0, 0, 1});
    CHECK(s.size() == 2);
    // L shape: two quads.
    Mesh l;
    l.verts = {{0, 0, 0}, {100, 0, 0}, {100, 40, 0}, {40, 40, 0}, {40, 100, 0}, {0, 100, 0}};
    auto ls = splitPolygon(l, {0, 1, 2, 3, 4, 5}, {0, 0, 1});
    CHECK(ls.size() == 2);
    for (auto& p : ls) CHECK(p.size() == 4);
    // Octagon: bands of quads.
    Mesh o;
    for (const Vec3& p : circlePoints({0, 0, 0}, {1, 0, 0}, {0, 1, 0}, 50, 8)) o.verts.push_back(p);
    auto os = splitPolygon(o, {0, 1, 2, 3, 4, 5, 6, 7}, {0, 0, 1});
    CHECK(os.size() == 3);
    // Clockwise: refused.
    CHECK(splitPolygon(m, {3, 2, 1, 0}, {0, 0, 1}).empty());
}

void testRectangleInside() {
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    const int top = faceFacing(m, {0, 0, 1});
    MeshEdit e(m);
    std::set<int> shape;
    std::string why;
    CHECK(cutShape(e, top, rectanglePoints({30, 30, 100}, {1, 0, 0}, {0, 1, 0}, 40, 20), true, &shape, &why));
    CHECK(why.empty());
    CHECK(shape.size() == 1);
    CHECK(near(faceArea(m, *shape.begin()), 800, 1e-6));
    CHECK(near(areaFacing(m, {0, 0, 1}), 10000, 1e-6));
    CHECK(isClosed(m) && near(signedVolume(m), 1e6, 1e-6));
    CHECK(goodFaces(m));
    // Pushed up: the shape goes as one, new side faces, volume grows by 800 x 10.
    CHECK(pushKind(m, shape, false) == PushKind::NewFaces);
    std::set<int> moved;
    CHECK(pushPullFaces(e, shape, 10, false, &moved));
    CHECK(isClosed(m) && near(signedVolume(m), 1e6 + 8000, 1e-6));
    // And in.
    Mesh r = box({0, 0, 0}, {100, 100, 100});
    MeshEdit er(r);
    shape.clear();
    CHECK(cutShape(er, top, rectanglePoints({30, 30, 100}, {1, 0, 0}, {0, 1, 0}, 40, 20), true, &shape));
    CHECK(pushPullFaces(er, shape, -10, false));
    CHECK(isClosed(r) && near(signedVolume(r), 1e6 - 8000, 1e-6));
}

void testCircleInside() {
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    const int top = faceFacing(m, {0, 0, 1});
    MeshEdit e(m);
    std::set<int> shape;
    CHECK(cutShape(e, top, circlePoints({50, 50, 100}, {1, 0, 0}, {0, 1, 0}, 30, 8), true, &shape));
    CHECK(shape.size() == 3);
    double a = 0;
    for (int f : shape) a += faceArea(m, f);
    CHECK(near(a, 2 * std::sqrt(2.0) * 900, 0.05)); // regular octagon, radius 30
    CHECK(near(areaFacing(m, {0, 0, 1}), 10000, 1e-3));
    CHECK(isClosed(m) && goodFaces(m));
    // 12 segments, off centre.
    Mesh c = box({0, 0, 0}, {100, 100, 100});
    MeshEdit ec(c);
    CHECK(cutShape(ec, top, circlePoints({30, 60, 100}, {1, 0, 0}, {0, 1, 0}, 20, 12), true, nullptr));
    CHECK(isClosed(c) && goodFaces(c) && near(areaFacing(c, {0, 0, 1}), 10000, 1e-3));
}

void testDoor() {
    // A door on the front from the floor: the bottom edge's neighbour (the floor) gets
    // the two new points; everything stays closed.
    Mesh m = box({0, 0, 0}, {200, 100, 250});
    const int front = faceFacing(m, {0, -1, 0});
    MeshEdit e(m);
    std::set<int> shape;
    std::string why;
    const std::vector<Vec3> door = {{80, 0, 0}, {120, 0, 0}, {120, 0, 200}, {80, 0, 200}};
    CHECK(cutShape(e, front, door, true, &shape, &why));
    CHECK(why.empty());
    double a = 0;
    for (int f : shape) a += faceArea(m, f);
    CHECK(near(a, 40 * 200, 1e-6));
    CHECK(near(areaFacing(m, {0, -1, 0}), 200 * 250, 1e-6));
    CHECK(near(areaFacing(m, {0, 0, -1}), 200 * 100, 1e-6));
    CHECK(isClosed(m) && goodFaces(m));
    CHECK(near(signedVolume(m), 200 * 100 * 250, 1e-3));
    // In a corner (two sides on the border).
    Mesh c = box({0, 0, 0}, {200, 100, 250});
    MeshEdit ec(c);
    CHECK(cutShape(ec, front, {{0, 0, 0}, {50, 0, 0}, {50, 0, 50}, {0, 0, 50}}, true, nullptr));
    CHECK(isClosed(c) && goodFaces(c));
}

void testLines() {
    // Straight across the top, front edge to back edge.
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    const int top = faceFacing(m, {0, 0, 1});
    MeshEdit e(m);
    std::set<int> part;
    CHECK(cutShape(e, top, {{30, 0, 100}, {30, 100, 100}}, false, &part));
    CHECK(part.size() == 1 && near(faceArea(m, *part.begin()), 3000, 1e-6)); // the smaller part
    CHECK(isClosed(m) && goodFaces(m));
    // Corner to corner: no new points.
    Mesh d = box({0, 0, 0}, {100, 100, 100});
    MeshEdit ed(d);
    const size_t v0 = d.verts.size();
    CHECK(cutShape(ed, top, {{0, 0, 100}, {100, 100, 100}}, false, nullptr));
    CHECK(d.verts.size() == v0 && isClosed(d) && goodFaces(d));
    // With a bend.
    Mesh b = box({0, 0, 0}, {100, 100, 100});
    MeshEdit eb(b);
    CHECK(cutShape(eb, top, {{0, 40, 100}, {50, 60, 100}, {100, 40, 100}}, false, nullptr));
    CHECK(isClosed(b) && goodFaces(b) && near(areaFacing(b, {0, 0, 1}), 10000, 1e-6));
}

void testRefused() {
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    const int top = faceFacing(m, {0, 0, 1});
    std::string why;
    {
        MeshEdit e(m);
        CHECK(!cutShape(e, top, rectanglePoints({80, 80, 100}, {1, 0, 0}, {0, 1, 0}, 40, 40), true, nullptr, &why));
        CHECK(!why.empty());
        e.revert();
    }
    {
        MeshEdit e(m);
        why.clear();
        CHECK(!cutShape(e, top, {{30, 0, 100}, {30, 50, 100}}, false, nullptr, &why)); // stops inside
        CHECK(!why.empty());
        e.revert();
    }
    {
        MeshEdit e(m);
        CHECK(!cutShape(e, top, {{0, 0, 100}, {100, 0, 100}}, false, nullptr)); // along the edge
        e.revert();
    }
    {
        MeshEdit e(m);
        // Touching the border in two places (a bar right across): refused for a shape.
        CHECK(!cutShape(e, top, {{0, 40, 100}, {100, 40, 100}, {100, 60, 100}, {0, 60, 100}}, true, nullptr));
        e.revert();
    }
    CHECK(isClosed(m) && m.faces.size() == 6);
}

void testTiltedFace() {
    // A face that isn't along an axis: a box turned 30° about Z, a circle on its side.
    Mesh m = box({0, 0, 0}, {100, 100, 100});
    double c, s;
    cosSinDeg(30, c, s);
    for (Vec3& p : m.verts) p = clean(Vec3{p.x * c - p.y * s, p.x * s + p.y * c, p.z});
    int side = -1;
    for (int i = 0; i < (int)m.faces.size(); ++i)
        if (dot(faceNormal(m, i), Vec3{s, -c, 0}) > 0.9999) side = i;
    CHECK(side >= 0);
    Vec3 o, u, v, n;
    CHECK(faceFrame(m, side, o, u, v, n));
    const Vec3 centre = faceCentre(m, side);
    MeshEdit e(m);
    std::string why;
    CHECK(cutShape(e, side, circlePoints(centre, u, v, 25, 8), true, nullptr, &why));
    CHECK(why.empty());
    CHECK(isClosed(m) && goodFaces(m));
}

void manyShapes(Mesh m, unsigned seed, int rounds);

void testManyShapes() {
    manyShapes(box({0, 0, 0}, {160, 120, 96}), 12345, 300);
    manyShapes(box({0, 0, 0}, {160, 120, 96}), 777, 300);
    PrimParams p;
    p.kind = PrimKind::Cylinder;
    manyShapes(makePrimitive(p, {0, 0, 0}, {120, 120, 100}, 2), 4242, 300);
}

void manyShapes(Mesh m, unsigned seed, int rounds) {
    // Many shapes on many faces, one after another on the same model: each cut either
    // works and leaves a closed box with good faces, or is refused and changes nothing.
    auto rnd = [&](int lo, int hi) {
        seed = seed * 1103515245u + 12345u;
        return lo + (int)((seed >> 8) % (unsigned)(hi - lo + 1));
    };
    const double vol = signedVolume(m);
    int done = 0, refused = 0, broken = 0;
    for (int round = 0; round < rounds; ++round) {
        const int fi = rnd(0, (int)m.faces.size() - 1);
        Vec3 o, u, v, n;
        if (!faceFrame(m, fi, o, u, v, n)) continue;
        // A point somewhere on the face (its centre plus a bit), snapped to whole cm.
        const Vec3 c = faceCentre(m, fi);
        std::vector<Vec3> pts;
        bool closed = true;
        switch (rnd(0, 2)) {
        case 0: pts = rectanglePoints(c + u * rnd(-20, 5) + v * rnd(-20, 5), u, v, rnd(1, 30), rnd(1, 30)); break;
        case 1: pts = circlePoints(c, u, v, rnd(2, 25), rnd(3, 12)); break;
        default: {
            // A line through the centre between two edges' midpoints.
            const Face& f = m.faces[fi];
            const int a = rnd(0, f.n - 1), b = (a + rnd(1, f.n - 1)) % f.n;
            pts = {(m.verts[f.v[a]] + m.verts[f.v[(a + 1) % f.n]]) * 0.5, c,
                   (m.verts[f.v[b]] + m.verts[f.v[(b + 1) % f.n]]) * 0.5};
            closed = false;
        }
        }
        Mesh before = m;
        MeshEdit e(m);
        if (cutShape(e, fi, pts, closed, nullptr)) {
            ++done;
            if (!isClosed(m) || !goodFaces(m) || !near(signedVolume(m), vol, 2e-6 * std::fabs(vol))) {
                ++broken;
                goodFaces(m, true);
                std::printf("round %d face %d closed %d good %d vol %.4f kind %s n=%zu\n", round, fi, isClosed(m), goodFaces(m),
                            signedVolume(m), closed ? "shape" : "line", pts.size());
                for (const Vec3& p : pts) std::printf("  %.3f %.3f %.3f\n", p.x, p.y, p.z);
                {
                    std::map<std::pair<int, int>, int> cnt;
                    for (const Face& g : m.faces)
                        for (int k = 0; k < g.n; ++k) ++cnt[{g.v[k], g.v[(k + 1) % g.n]}];
                    for (const auto& [d, c] : cnt)
                        if (c != 1 || !cnt.count({d.second, d.first}))
                            std::printf("  open %d->%d (%g %g %g)->(%g %g %g) x%d\n", d.first, d.second, m.verts[d.first].x,
                                        m.verts[d.first].y, m.verts[d.first].z, m.verts[d.second].x, m.verts[d.second].y,
                                        m.verts[d.second].z, c);
                }
                const Face& f = before.faces[fi];
                for (int k = 0; k < f.n; ++k) std::printf("  face %.3f %.3f %.3f\n", before.verts[f.v[k]].x, before.verts[f.v[k]].y, before.verts[f.v[k]].z);
            }
        } else {
            e.revert();
            ++refused;
            if (m.faces.size() != before.faces.size() || m.verts.size() != before.verts.size()) ++broken;
        }
        if (broken) break;
    }
    CHECK(broken == 0);
    CHECK(done > 30);
    (void)refused;
}

} // namespace

void testDrawOps() {
    testManyShapes();
    testSplitPolygon();
    testRectangleInside();
    testCircleInside();
    testDoor();
    testLines();
    testRefused();
    testTiltedFace();
}
