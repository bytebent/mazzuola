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

// Open surface check for flat outlines: every edge used once, and inside edges in both
// directions; returns the number of border edges.
int borderEdges(const Mesh& m) {
    std::map<std::pair<int, int>, int> cnt;
    for (const Face& f : m.faces)
        for (int k = 0; k < f.n; ++k) ++cnt[{f.v[k], f.v[(k + 1) % f.n]}];
    int border = 0;
    for (const auto& [d, c] : cnt) {
        if (c != 1) return -1;
        if (!cnt.count({d.second, d.first})) ++border;
    }
    return border;
}

double totalArea(const Mesh& m) {
    double a = 0;
    for (int i = 0; i < (int)m.faces.size(); ++i) a += faceArea(m, i);
    return a;
}

void testWindowThrough() {
    // A wall 200 x 20 x 150: a window drawn on the front, pushed through to the back.
    Mesh m = box({0, 0, 0}, {200, 20, 150});
    const int front = faceFacing(m, {0, -1, 0});
    MeshEdit e(m);
    std::set<int> shape;
    CHECK(cutShape(e, front, rectanglePoints({60, 0, 50}, {1, 0, 0}, {0, 0, 1}, 80, 60), true, &shape));
    int target = -1;
    CHECK(near(throughDistance(m, shape, &target), 20, 1e-9));
    CHECK(target >= 0 && nearV(faceNormal(m, target), {0, 1, 0}, 1e-9));
    std::string why;
    CHECK(pushThrough(e, shape, &why));
    CHECK(why.empty());
    CHECK(isClosed(m) && goodFaces(m));
    CHECK(near(signedVolume(m), 200 * 20 * 150 - 80 * 60 * 20, 1e-3));
    // A circle through a slab, from the top.
    Mesh s = box({0, 0, 0}, {100, 100, 10});
    const int top = faceFacing(s, {0, 0, 1});
    MeshEdit es(s);
    shape.clear();
    CHECK(cutShape(es, top, circlePoints({50, 50, 10}, {1, 0, 0}, {0, 1, 0}, 20, 8), true, &shape));
    CHECK(near(throughDistance(s, shape), 10, 1e-9));
    CHECK(pushThrough(es, shape));
    CHECK(isClosed(s) && goodFaces(s));
    // A doorway touching the floor: the wall's underside opens under it.
    Mesh d = box({0, 0, 0}, {200, 20, 250});
    const int df = faceFacing(d, {0, -1, 0});
    MeshEdit ed(d);
    shape.clear();
    CHECK(cutShape(ed, df, {{80, 0, 0}, {120, 0, 0}, {120, 0, 200}, {80, 0, 200}}, true, &shape));
    CHECK(near(throughDistance(d, shape), 20, 1e-9));
    why.clear();
    CHECK(pushThrough(ed, shape, &why));
    CHECK(why.empty());
    CHECK(isClosed(d) && goodFaces(d));
    CHECK(near(signedVolume(d), 200 * 20 * 250 - 40 * 200 * 20, 1e-3));
    CHECK(near(areaFacing(d, {0, 0, -1}), 160 * 20 + 40 * 20, 1e-6)); // floor either side + the top of the opening
    // In a corner (floor and side both open).
    Mesh k = box({0, 0, 0}, {200, 20, 250});
    MeshEdit ek(k);
    shape.clear();
    CHECK(cutShape(ek, faceFacing(k, {0, -1, 0}), {{0, 0, 0}, {50, 0, 0}, {50, 0, 100}, {0, 0, 100}}, true, &shape));
    why.clear();
    CHECK(pushThrough(ek, shape, &why));
    CHECK(why.empty());
    CHECK(isClosed(k) && goodFaces(k));
    CHECK(near(signedVolume(k), 200 * 20 * 250 - 50 * 100 * 20, 1e-3));
}

void testThroughRandom() {
    // Random rectangles and circles on a wall's front (some touching its border), pushed
    // through: closed, good faces, the right volume; or refused, changing nothing.
    unsigned seed = 2024;
    auto rnd = [&](int lo, int hi) {
        seed = seed * 1103515245u + 12345u;
        return lo + (int)((seed >> 8) % (unsigned)(hi - lo + 1));
    };
    int through = 0, bad = 0;
    for (int round = 0; round < 300; ++round) {
        Mesh m = box({0, 0, 0}, {120, 16, 90});
        const double v0 = signedVolume(m);
        const int front = faceFacing(m, {0, -1, 0});
        MeshEdit e(m);
        std::set<int> shape;
        std::vector<Vec3> pts;
        double area = 0;
        if (rnd(0, 1)) {
            const double x = rnd(0, 100), z = rnd(0, 70), w = rnd(4, 120 - (int)x), h = rnd(4, 90 - (int)z);
            pts = rectanglePoints({x, 0, z}, {1, 0, 0}, {0, 0, 1}, w, h);
            area = w * h;
        } else {
            const double r = rnd(4, 30);
            const int segs = rnd(3, 12);
            pts = circlePoints({(double)rnd(35, 85), 0, (double)rnd(35, 55)}, {1, 0, 0}, {0, 0, 1}, r, segs);
            area = 0.5 * segs * r * r * std::sin(2 * M_PI / segs);
        }
        if (!cutShape(e, front, pts, true, &shape)) continue;
        if (throughDistance(m, shape) <= 0) continue;
        std::string why;
        const size_t faces = m.faces.size();
        if (pushThrough(e, shape, &why)) {
            ++through;
            if (!isClosed(m) || !goodFaces(m) || !near(signedVolume(m), v0 - area * 16, 2.0)) {
                ++bad;
                std::printf("through round %d closed %d good %d vol %.3f want %.3f\n", round, isClosed(m), goodFaces(m),
                            signedVolume(m), v0 - area * 16);
            }
        } else if (m.faces.size() != faces) {
            // A refusal may have changed the model inside its MeshEdit: the caller reverts.
        }
    }
    CHECK(bad == 0);
    CHECK(through > 100);
}

void testWallWithManyHoles() {
    // One wall, many windows and doorways one after another (each drawn on the surface
    // as it is by then, pieces and holes and all), each pushed through.
    unsigned seed = 31337;
    auto rnd = [&](int lo, int hi) {
        seed = seed * 1103515245u + 12345u;
        return lo + (int)((seed >> 8) % (unsigned)(hi - lo + 1));
    };
    Mesh m = box({0, 0, 0}, {400, 16, 120});
    int holes = 0, bad = 0;
    for (int round = 0; round < 200 && !bad; ++round) {
        // Any face facing front lying on the wall's front plane.
        int front = -1;
        const double x = rnd(0, 380), z = rnd(0, 100), w = rnd(4, 40), h = rnd(4, 60);
        const Vec3 probe{x + w / 2, 0, z + h / 2};
        for (int i = 0; i < (int)m.faces.size(); ++i)
            if (nearV(faceNormal(m, i), {0, -1, 0}, 1e-9) && std::fabs(m.verts[m.faces[i].v[0]].y) < 1e-9) front = i;
        if (front < 0) break;
        (void)probe;
        const double v0 = signedVolume(m);
        Mesh before = m;
        MeshEdit e(m);
        std::set<int> shape;
        if (!cutShape(e, front, rectanglePoints({x, 0, z}, {1, 0, 0}, {0, 0, 1}, w, h), true, &shape) ||
            throughDistance(m, shape) <= 0 || !pushThrough(e, shape)) {
            e.revert();
            continue;
        }
        ++holes;
        if (!isClosed(m) || !goodFaces(m) || signedVolume(m) > v0 - 1) {
            ++bad;
            std::printf("wall round %d closed %d good %d\n", round, isClosed(m), goodFaces(m));
        }
    }
    CHECK(bad == 0);
    CHECK(holes > 10);
}

void testOutline() {
    // A plain L, facing up.
    Mesh m;
    MeshEdit e(m);
    std::set<int> faces;
    std::string why;
    const std::vector<Vec3> l = {{0, 0, 0}, {100, 0, 0}, {100, 40, 0}, {40, 40, 0}, {40, 100, 0}, {0, 100, 0}};
    CHECK(makeOutline(e, l, {}, {0, 0, 1}, false, &faces, &why));
    CHECK(why.empty() && faces.size() == 2);
    CHECK(near(totalArea(m), 100 * 40 + 40 * 60, 1e-9));
    for (int f : faces) CHECK(nearV(faceNormal(m, f), {0, 0, 1}, 1e-9));
    CHECK(borderEdges(m) == 6 && goodFaces(m));
    // Drawn clockwise, facing down: still faces the way asked.
    Mesh cw;
    MeshEdit ecw(cw);
    std::vector<Vec3> rl(l.rbegin(), l.rend());
    CHECK(makeOutline(ecw, rl, {}, {0, 0, -1}, true, &faces));
    for (int f : faces) CHECK(nearV(faceNormal(cw, f), {0, 0, -1}, 1e-9) && cw.faces[f].twoSided);
    // A square with a square hole (a frame), and with two holes.
    Mesh h;
    MeshEdit eh(h);
    const std::vector<Vec3> sq = {{0, 0, 0}, {100, 0, 0}, {100, 100, 0}, {0, 100, 0}};
    CHECK(makeOutline(eh, sq, {{{30, 30, 0}, {70, 30, 0}, {70, 70, 0}, {30, 70, 0}}}, {0, 0, 1}, false, &faces, &why));
    CHECK(near(totalArea(h), 10000 - 1600, 1e-9));
    CHECK(borderEdges(h) == 8 && goodFaces(h));
    Mesh h2;
    MeshEdit eh2(h2);
    CHECK(makeOutline(eh2, sq,
                      {{{10, 10, 0}, {40, 10, 0}, {40, 40, 0}, {10, 40, 0}},
                       circlePoints({70, 70, 0}, {1, 0, 0}, {0, 1, 0}, 15, 8)},
                      {0, 0, 1}, false, &faces, &why));
    CHECK(near(totalArea(h2), 10000 - 900 - 2 * std::sqrt(2.0) * 225, 0.05));
    CHECK(borderEdges(h2) == 4 + 4 + 8 && goodFaces(h2));
    // Pulled up into a solid: closed, with its holes.
    std::set<int> moved;
    CHECK(pushKind(h2, faces, false) == PushKind::Solid);
    CHECK(pushPullFaces(eh2, faces, 20, false, &moved));
    CHECK(isClosed(h2) && goodFaces(h2));
    CHECK(near(signedVolume(h2), (10000 - 900 - 2 * std::sqrt(2.0) * 225) * 20, 1));
    // Refused: crossing itself, a hole outside, holes crossing.
    Mesh r;
    MeshEdit er(r);
    CHECK(!makeOutline(er, {{0, 0, 0}, {100, 100, 0}, {100, 0, 0}, {0, 100, 0}}, {}, {0, 0, 1}, false, nullptr, &why));
    CHECK(!makeOutline(er, sq, {{{150, 10, 0}, {160, 10, 0}, {160, 20, 0}}}, {0, 0, 1}, false, nullptr));
    CHECK(!makeOutline(er, sq,
                       {{{10, 10, 0}, {50, 10, 0}, {50, 50, 0}, {10, 50, 0}},
                        {{30, 30, 0}, {70, 30, 0}, {70, 70, 0}, {30, 70, 0}}},
                       {0, 0, 1}, false, nullptr));
}

void testOutlineRandom() {
    // Random star-shaped outlines with a few random holes: faces or a clean refusal.
    unsigned seed = 99;
    auto rnd = [&](int lo, int hi) {
        seed = seed * 1103515245u + 12345u;
        return lo + (int)((seed >> 8) % (unsigned)(hi - lo + 1));
    };
    int made = 0, bad = 0;
    for (int round = 0; round < 300; ++round) {
        std::vector<Vec3> outer;
        const int n = rnd(3, 12);
        for (int i = 0; i < n; ++i) {
            double c, s;
            cosSinDeg(360.0 * i / n + rnd(-10, 10), c, s);
            const double r = rnd(60, 120);
            outer.push_back({std::round(r * c), std::round(r * s), 0});
        }
        std::vector<std::vector<Vec3>> holes;
        for (int k = rnd(0, 3); k > 0; --k) {
            const double cx = rnd(-30, 30), cy = rnd(-30, 30);
            holes.push_back(circlePoints({cx, cy, 0}, {1, 0, 0}, {0, 1, 0}, rnd(4, 15), rnd(3, 8)));
        }
        Mesh m;
        MeshEdit e(m);
        std::set<int> faces;
        if (makeOutline(e, outer, holes, {0, 0, 1}, false, &faces)) {
            ++made;
            int border = 0;
            border += (int)outer.size();
            for (const auto& h : holes) border += (int)h.size();
            if (borderEdges(m) != border || !goodFaces(m)) {
                ++bad;
                std::printf("outline round %d border %d want %d good %d\n", round, borderEdges(m), border, goodFaces(m));
            }
        }
    }
    CHECK(bad == 0);
    CHECK(made > 100);
}

} // namespace

void testDrawOps() {
    testWindowThrough();
    testThroughRandom();
    testWallWithManyHoles();
    testOutline();
    testOutlineRandom();
    testManyShapes();
    testSplitPolygon();
    testRectangleInside();
    testCircleInside();
    testDoor();
    testLines();
    testRefused();
    testTiltedFace();
}
