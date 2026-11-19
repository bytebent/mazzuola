// Core tests for the model: clean numbers, mesh, pieces, selection, document and undo.
#include "check.hpp"
#include "core/document.hpp"
#include "core/numbers.hpp"
#include <cstdlib>
#include <map>
#include <string>

using namespace mazz;

namespace {

bool sameMesh(const Mesh& a, const Mesh& b) {
    if (a.verts.size() != b.verts.size() || a.faces.size() != b.faces.size()) return false;
    for (size_t i = 0; i < a.verts.size(); ++i)
        if (a.verts[i].x != b.verts[i].x || a.verts[i].y != b.verts[i].y || a.verts[i].z != b.verts[i].z) return false;
    for (size_t i = 0; i < a.faces.size(); ++i)
        if (a.faces[i] != b.faces[i]) return false;
    return true;
}

// Every edge used exactly twice, once in each direction: closed and consistently wound.
bool closedAndConsistent(const Mesh& m) {
    std::map<std::pair<int, int>, int> uses;
    for (const Face& f : m.faces)
        for (int k = 0; k < f.n; ++k) ++uses[{f.v[k], f.v[(k + 1) % f.n]}];
    for (const auto& [e, n] : uses)
        if (n != 1 || uses.count({e.second, e.first}) == 0) return false;
    return true;
}

double signedVolume(const Mesh& m) {
    double v = 0;
    for (const Face& f : m.faces)
        for (int k = 1; k + 1 < f.n; ++k)
            v += dot(m.verts[f.v[0]], cross(m.verts[f.v[k]], m.verts[f.v[k + 1]])) / 6;
    return v;
}

void testNumbers() {
    CHECK(clean(0.1 + 0.2) == clean(0.3));
    CHECK(clean(3.061616997868383e-15) == 0);
    CHECK(!std::signbit(clean(-1e-9)));
    CHECK(clean(62.499999999999986) == 62.5);
    CHECK(clean(1.0005) == 1.001 || clean(1.0005) == 1.0); // halfway: either neighbour is fine
    CHECK(isClean(62.5) && isClean(0) && !isClean(0.0001) && !isClean(-0.0));

    CHECK(formatClean(100) == "100");
    CHECK(formatClean(50) == "50");
    CHECK(formatClean(1000) == "1000");
    CHECK(formatClean(1000000) == "1000000");
    CHECK(formatClean(0.5) == "0.5");
    CHECK(formatClean(-0.125) == "-0.125");
    CHECK(formatClean(62.499999999999986) == "62.5");
    CHECK(formatClean(3.061616997868383e-15) == "0");
    CHECK(formatClean(-0.0) == "0");
    CHECK(formatClean(-0.0004) == "0");
    CHECK(formatClean(21.650635094610966) == "21.651");

    // What is written reads back as exactly the same clean value.
    std::srand(7);
    bool roundTrip = true, noExponent = true;
    for (int i = 0; i < 20000; ++i) {
        const double v = clean((std::rand() - RAND_MAX / 2) / 997.0 * (i % 7 + 1));
        const std::string s = formatClean(v);
        if (std::strtod(s.c_str(), nullptr) != v) roundTrip = false;
        if (s.find('e') != std::string::npos || s.find('E') != std::string::npos) noExponent = false;
    }
    CHECK(roundTrip);
    CHECK(noExponent);

    double c = 0, s = 0;
    cosSinDeg(90, c, s);
    CHECK(c == 0 && s == 1);
    cosSinDeg(-90, c, s);
    CHECK(c == 0 && s == -1);
    cosSinDeg(180, c, s);
    CHECK(c == -1 && s == 0);
    cosSinDeg(450, c, s);
    CHECK(c == 0 && s == 1);
    cosSinDeg(45, c, s);
    CHECK(near(c, std::sqrt(0.5)) && near(s, std::sqrt(0.5)));
}

void testBox() {
    Mesh m;
    addBox(m, {64, 0.1 + 0.2, 0}, {0, 32, 48});
    CHECK(m.verts.size() == 8);
    CHECK(m.faces.size() == 6);
    CHECK(closedAndConsistent(m));
    CHECK(signedVolume(m) > 0); // faces point outward
    bool allClean = true;
    for (const Vec3& p : m.verts)
        for (int a = 0; a < 3; ++a) allClean = allClean && isClean(p[a]);
    CHECK(allClean);
    Vec3 mn, mx;
    CHECK(m.bounds(mn, mx));
    CHECK(mn.x == 0 && mn.y == 0.3 && mn.z == 0 && mx.x == 64 && mx.y == 32 && mx.z == 48);
    // Each face's normal points away from the box centre.
    const Vec3 centre = (mn + mx) / 2;
    bool outward = true;
    for (int f = 0; f < 6; ++f) outward = outward && dot(faceNormal(m, f), faceCentre(m, f) - centre) > 0;
    CHECK(outward);
}

void testPieces() {
    Mesh m;
    addBox(m, {0, 0, 0}, {10, 10, 10});
    addBox(m, {20, 0, 0}, {30, 10, 10});
    std::vector<int> piece;
    CHECK(pieceIds(m, piece) == 2);
    CHECK(piece[0] == 0 && piece[5] == 0 && piece[6] == 1 && piece[11] == 1);
    const std::set<int> whole = wholePieces(m, {7});
    CHECK(whole.size() == 6 && *whole.begin() == 6);

    Selection s;
    CHECK(s.mode == SelectMode::Piece);
    s.faces = whole;
    Vec3 mn, mx;
    CHECK(selectionBounds(m, s, mn, mx));
    CHECK(mn.x == 20 && mx.x == 30);
    CHECK(selectedVerts(m, s).size() == 8);
}

void testDocument() {
    Document d;
    CHECK(!d.dirty() && !d.canUndo());

    // A new box: one step, selected as a piece.
    CHECK(d.apply("New box", [](MeshEdit& e, Selection& s) {
        Mesh box;
        addBox(box, {0, 0, 0}, {64, 64, 64});
        for (const Vec3& p : box.verts) e.addVert(p);
        for (const Face& f : box.faces) s.faces.insert(e.addFace(f));
        return true;
    }));
    CHECK(d.mesh.faces.size() == 6 && d.sel.faces.size() == 6 && d.dirty());
    const Mesh afterBox = d.mesh;
    d.markSaved();
    CHECK(!d.dirty());

    // A cancelled command leaves nothing behind.
    CHECK(!d.apply("Nothing", [](MeshEdit& e, Selection&) {
        e.setVert(0, {999, 0, 0});
        return false;
    }));
    CHECK(sameMesh(d.mesh, afterBox) && d.undoCount() == 1);

    // A command that changes nothing is not a step.
    CHECK(!d.apply("Same", [](MeshEdit& e, Selection&) {
        e.setVert(0, e.mesh().verts[0]);
        return true;
    }));
    CHECK(d.undoCount() == 1);

    // A live drag: many updates, one step, only the moved points stored.
    d.beginLive();
    const std::set<int> moved = selectedVerts(d.mesh, d.sel);
    std::map<int, Vec3> start;
    for (int v : moved) start[v] = d.mesh.verts[v];
    for (int frame = 1; frame <= 10; ++frame)
        for (int v : moved) d.live().setVert(v, start[v] + Vec3{frame * 1.0, 0, 0.1 + 0.2});
    CHECK(d.commitLive("Move"));
    CHECK(d.undoCount() == 2 && d.dirty());
    CHECK(d.mesh.verts[0].x == 10 && d.mesh.verts[0].z == 0.3); // cleaned
    const Mesh afterMove = d.mesh;

    // Switching mode is not a step; undo brings back the selection with its mode.
    Selection pts;
    pts.mode = SelectMode::Vert;
    pts.verts = {0, 1};
    d.select(pts);
    CHECK(d.undo());
    CHECK(sameMesh(d.mesh, afterBox));
    CHECK(d.sel.mode == SelectMode::Piece && d.sel.faces.size() == 6);
    CHECK(!d.dirty()); // back to what was saved
    CHECK(d.redo());
    CHECK(sameMesh(d.mesh, afterMove) && d.dirty());
    CHECK(d.sel.mode == SelectMode::Piece && d.sel.faces.size() == 6);

    // A cancelled live edit puts everything back.
    d.beginLive();
    d.live().setVert(3, {-50, -50, -50});
    d.select(pts);
    d.cancelLive();
    CHECK(sameMesh(d.mesh, afterMove) && d.sel.mode == SelectMode::Piece && d.undoCount() == 2);

    // Delete one of two boxes: points and faces renumbered, undo exact.
    CHECK(d.apply("Second box", [](MeshEdit& e, Selection& s) {
        Mesh box;
        addBox(box, {100, 0, 0}, {120, 20, 20});
        const int base = (int)e.mesh().verts.size();
        for (const Vec3& p : box.verts) e.addVert(p);
        s.faces.clear();
        for (Face f : box.faces) {
            for (int k = 0; k < f.n; ++k) f.v[k] += base;
            s.faces.insert(e.addFace(f));
        }
        return true;
    }));
    const Mesh twoBoxes = d.mesh;
    CHECK(d.apply("Delete", [](MeshEdit& e, Selection& s) {
        e.deleteFaces(wholePieces(e.mesh(), {0}));
        s.faces.clear();
        return true;
    }));
    CHECK(d.mesh.verts.size() == 8 && d.mesh.faces.size() == 6);
    CHECK(closedAndConsistent(d.mesh));
    Vec3 mn, mx;
    d.mesh.bounds(mn, mx);
    CHECK(mn.x == 100 && mx.x == 120);
    CHECK(d.undo());
    CHECK(sameMesh(d.mesh, twoBoxes));
    CHECK(d.redo());
    CHECK(d.mesh.faces.size() == 6);

    // Moving one point of a big model stores a tiny step.
    Document big;
    big.apply("Many boxes", [](MeshEdit& e, Selection&) {
        for (int i = 0; i < 500; ++i) {
            Mesh box;
            addBox(box, {i * 10.0, 0, 0}, {i * 10.0 + 5, 5, 5});
            const int base = (int)e.mesh().verts.size();
            for (const Vec3& p : box.verts) e.addVert(p);
            for (Face f : box.faces) {
                for (int k = 0; k < f.n; ++k) f.v[k] += base;
                e.addFace(f);
            }
        }
        return true;
    });
    const std::size_t before = big.historyBytes();
    big.apply("Nudge", [](MeshEdit& e, Selection&) {
        e.setVert(17, e.mesh().verts[17] + Vec3{1, 0, 0});
        return true;
    });
    CHECK(big.historyBytes() - before < 1024);

    // New model: empty history, clean.
    d.reset();
    CHECK(!d.canUndo() && !d.canRedo() && !d.dirty() && d.mesh.empty());
}

} // namespace

void testViewGeom();
void testDrawData();
void testTransformPick();
void testPrimitives();
void testSelectOps();

void testModel() {
    testSelectOps();
    testPrimitives();
    testViewGeom();
    testDrawData();
    testTransformPick();
    testNumbers();
    testBox();
    testPieces();
    testDocument();
}

// ---------------------------------------------------------------- view geometry
#include "core/view_geom.hpp"

void testViewGeom() {
    using namespace mazz;
    // 3D: project agrees with the camera's own matrices, and a ray through the projected
    // point passes through the point.
    ViewGeom g;
    g.dir = ViewDir::Persp;
    g.x0 = 100;
    g.y0 = 50;
    g.w = 800;
    g.h = 600;
    g.cam.yaw = 30;
    g.cam.pitch = 20;
    g.cam.distance = 500;
    const Vec3 p{40, -25, 60};
    double sx = 0, sy = 0;
    CHECK(g.project(p, sx, sy));
    const Mat4 vp = g.cam.projection(g.w / g.h) * g.cam.view();
    const Vec3 ndc = transformPoint(vp, p);
    CHECK(near(sx, g.x0 + (ndc.x + 1) / 2 * g.w, 0.05) && near(sy, g.y0 + (1 - ndc.y) / 2 * g.h, 0.05));
    Vec3 o, d;
    g.ray(sx, sy, o, d);
    const Vec3 toP = p - o;
    CHECK(near(length(cross(normalize(toP), d)), 0, 1e-6) && dot(toP, d) > 0);
    CHECK(!g.project(g.cam.eye() - g.cam.forward() * 10, sx, sy)); // behind the camera

    // 2D Top view: a ray goes straight down through the point under the cursor.
    ViewGeom t;
    t.dir = ViewDir::Top;
    t.w = 400;
    t.h = 300;
    t.ortho.dir = ViewDir::Top;
    CHECK(t.project({10, 20, 99}, sx, sy));
    t.ray(sx, sy, o, d);
    CHECK(nearV(d, {0, 0, -1}) && near(o.x, 10, 1e-6) && near(o.y, 20, 1e-6) && o.z > 1000);
    // Front view looks along +y.
    ViewGeom f = t;
    f.dir = ViewDir::Front;
    f.ortho.dir = ViewDir::Front;
    f.ray(200, 150, o, d);
    CHECK(nearV(d, {0, 1, 0}));

    double tt = 0;
    CHECK(rayTriangle({0.2, 0.2, 5}, {0, 0, -1}, {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, tt) && near(tt, 5));
    CHECK(!rayTriangle({2, 2, 5}, {0, 0, -1}, {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, tt));
}

// ---------------------------------------------------------------- drawing data, versions
#include "core/draw_data.hpp"

void testDrawData() {
    using namespace mazz;
    Mesh m;
    addBox(m, {0, 0, 0}, {10, 10, 10});
    addBox(m, {20, 0, 0}, {30, 10, 10});
    DrawData d;
    buildDrawData(m, d);
    CHECK(d.tris.size() == 12 * 6);   // 2 boxes x 6 quads x 2 triangles x 3
    CHECK(d.edgeList.size() == 24);   // 12 edges per box, each once
    CHECK(d.edges.size() == 48);
    // Normals point outward (the top face's normal is +z).
    CHECK(d.tris[d.faceFirst[1]].nz == 1.0f);
    Selection s;
    s.faces = {6, 7, 8, 9, 10, 11};
    CHECK(selectedTriIndices(d, s).size() == 36);
    CHECK(selectedEdgeIndices(m, d, s).size() == 24);
    s.mode = SelectMode::Vert;
    s.verts = {0, 1};
    CHECK(selectedTriIndices(d, s).empty());
    CHECK(selectedEdgeIndices(m, d, s).size() == 2);

    // Versions: the geometry counter moves with every change, undo and redo; selection
    // changes only move the selection counter.
    Document doc;
    const long m0 = doc.meshVersion(), s0 = doc.selVersion();
    Selection none;
    none.mode = SelectMode::Piece;
    doc.select(none); // same as before: nothing moves
    CHECK(doc.meshVersion() == m0 && doc.selVersion() == s0);
    doc.apply("Box", [](MeshEdit& e, Selection& sel) {
        e.addVert({0, 0, 0});
        e.addVert({1, 0, 0});
        e.addVert({0, 1, 0});
        Face f;
        f.n = 3;
        f.v[0] = 0; f.v[1] = 1; f.v[2] = 2;
        sel.faces.insert(e.addFace(f));
        return true;
    });
    CHECK(doc.meshVersion() > m0 && doc.selVersion() > s0);
    const long m1 = doc.meshVersion();
    Selection other;
    doc.select(other);
    CHECK(doc.meshVersion() == m1);
    doc.beginLive();
    doc.live().setVert(0, {5, 5, 5});
    CHECK(doc.meshVersion() > m1);
    const long m2 = doc.meshVersion();
    doc.cancelLive();
    CHECK(doc.meshVersion() > m2);
    const long m3 = doc.meshVersion();
    doc.undo();
    CHECK(doc.meshVersion() > m3);
}

// ---------------------------------------------------------------- transform, picking
#include "core/pick.hpp"
#include "core/transform.hpp"

void testTransformPick() {
    using namespace mazz;
    // Stretch: the right side pushed from 64 to 96, the left stays.
    const Vec3 p = stretchInBox({32, 10, 5}, {0, 0, 0}, {64, 20, 10}, {0, 0, 0}, {96, 20, 10});
    CHECK(p.x == 48 && p.y == 10 && p.z == 5);
    // A flat box just moves along its flat axis.
    const Vec3 q = stretchInBox({5, 5, 0}, {0, 0, 0}, {10, 10, 0}, {0, 0, 7}, {20, 10, 7});
    CHECK(q.x == 10 && q.z == 7);
    // Rotation: exact at 90°, counter-clockwise seen from +z.
    const Vec3 r = rotateAbout({10, 0, 3}, {0, 0, 0}, {0, 0, 1}, 90);
    CHECK(r.x == 0 && r.y == 10 && r.z == 3);
    const Vec3 r2 = rotateAbout({10, 0, 0}, {5, 5, 0}, {0, 0, 1}, 180);
    CHECK(r2.x == 0 && r2.y == 10);
    const Vec3 r3 = rotateAbout({10, 0, 0}, {0, 0, 0}, {0, 0, 1}, 30);
    CHECK(near(r3.x, 10 * std::sqrt(3.0) / 2) && near(r3.y, 5));
    CHECK(snapAngle(22, 15) == 15 && snapAngle(23, 15) == 30 && snapAngle(-8, 15) == -15 && snapAngle(7.3, 0) == 7.3);

    // Copying a box's faces makes a separate piece of 8 points and 6 faces.
    Document d;
    d.apply("Box", [](MeshEdit& e, Selection&) {
        Mesh b;
        addBox(b, {0, 0, 0}, {10, 10, 10});
        for (const Vec3& v : b.verts) e.addVert(v);
        for (const Face& f : b.faces) e.addFace(f);
        return true;
    });
    std::set<int> made;
    d.apply("Copy", [&](MeshEdit& e, Selection&) {
        made = copyFaces(e, {0, 1, 2, 3, 4, 5});
        return true;
    });
    CHECK(made.size() == 6 && d.mesh.verts.size() == 16 && d.mesh.faces.size() == 12);
    std::vector<int> piece;
    CHECK(pieceIds(d.mesh, piece) == 2);

    // Picking: a Top view and a 3D view over two boxes.
    Mesh m;
    addBox(m, {0, 0, 0}, {10, 10, 10});     // piece 0, low
    addBox(m, {5, 5, 20}, {15, 15, 30});   // piece 1, higher up, overlapping from above
    ViewGeom top;
    top.dir = ViewDir::Top;
    top.ortho.dir = ViewDir::Top;
    top.ortho.zoom = 10;
    top.w = 400;
    top.h = 400;
    double sx, sy;
    top.project({7, 7, 0}, sx, sy);
    std::vector<int> under = piecesAt(m, top, sx, sy);
    CHECK(under.size() == 2 && under[0] == 1 && under[1] == 0); // the higher one first
    top.project({2, 2, 0}, sx, sy);
    under = piecesAt(m, top, sx, sy);
    CHECK(under.size() == 1 && under[0] == 0);
    top.project({40, 40, 0}, sx, sy);
    CHECK(piecesAt(m, top, sx, sy).empty());
    // Rectangle: only pieces entirely inside.
    double ax, ay, bx, by;
    top.project({-1, -1, 0}, ax, ay);
    top.project({11, 11, 0}, bx, by);
    std::set<int> inRect = piecesInRect(m, top, ax, ay, bx, by);
    CHECK(inRect.size() == 1 && inRect.count(0));
    CHECK(facesOfPieces(m, inRect).size() == 6);

    ViewGeom g;
    g.dir = ViewDir::Persp;
    g.w = 800;
    g.h = 600;
    g.cam.target = {5, 5, 5};
    g.cam.distance = 200;
    g.project({5, 5, 10}, sx, sy); // top of the low box, seen from above at an angle
    under = piecesAt(m, g, sx, sy);
    CHECK(under.size() == 1);
}

// ---------------------------------------------------------------- primitives
#include "core/primitives.hpp"

void testPrimitives() {
    using namespace mazz;
    const Vec3 mn{-24, 8, 0}, mx{40, 56, 32};
    for (int k = 0; k < kPrimKindCount; ++k)
        for (int axis = 0; axis < 3; ++axis)
            for (int variant = 0; variant < 3; ++variant) {
                PrimParams p;
                p.kind = (PrimKind)k;
                p.sides = variant == 0 ? 8 : variant == 1 ? 6 : 5; // also odd counts and non-multiples of 4
                p.tubeSides = variant == 0 ? 6 : 7;
                p.stacks = variant == 2 ? 5 : 6;
                p.segments = variant + 1;
                p.capRings = variant + 1;
                p.segX = variant + 1;
                p.segY = 2;
                p.segZ = variant == 2 ? 3 : 1;
                p.rows = variant + 1;
                p.cols = 2;
                p.iterations = variant;
                p.rock.faces = variant == 0 ? 12 : variant == 1 ? 24 : 60;
                p.rock.roughness = variant * 50;
                p.rock.flatBottom = variant != 1;
                p.rock.joinQuads = variant == 2;
                p.rock.seed = 7 + variant + axis * 3;
                p.roundSegments = variant; // sharp, 1 and 2 faces per edge
                p.roundSize = 20 + variant * 30;
                const Mesh m = makePrimitive(p, mn, mx, axis);
                const char* name = primKindName(p.kind);
                bool ok = !m.faces.empty();
                for (const Face& f : m.faces)
                    for (int c = 0; c < f.n; ++c) ok = ok && f.v[c] >= 0 && f.v[c] < (int)m.verts.size();
                bool clean_ = true;
                for (const Vec3& q : m.verts)
                    for (int a = 0; a < 3; ++a) clean_ = clean_ && isClean(q[a]);
                Vec3 bmn, bmx;
                m.bounds(bmn, bmx);
                Vec3 want = mx;
                if (p.kind == PrimKind::Plane) want[axis] = mn[axis];
                const bool fits = nearV(bmn, mn, 0) && nearV(bmx, want, 0);
                bool noFlat = true; // no face with zero area
                for (int f = 0; f < (int)m.faces.size(); ++f) noFlat = noFlat && length(faceNormal(m, f)) > 0.5;
                const bool closed = p.kind == PrimKind::Plane || closedAndConsistent(m);
                const bool outward = p.kind == PrimKind::Plane || signedVolume(m) > 0;
                CHECK(ok && clean_ && fits && noFlat && closed && outward);
                if (!(ok && clean_ && fits && noFlat && closed && outward))
                    std::fprintf(stderr, "  %s axis %d variant %d: ok %d clean %d fits %d noFlat %d closed %d outward %d\n",
                                 name, axis, variant, ok, clean_, fits, noFlat, closed, outward);
                std::vector<int> piece;
                CHECK(pieceIds(m, piece) == 1);
            }

    // Counts, and a plane faces along +axis.
    PrimParams p;
    p.kind = PrimKind::Cylinder;
    p.sides = 8;
    p.segments = 2;
    p.capRings = 1;
    Mesh m = makePrimitive(p, {0, 0, 0}, {10, 10, 10}, 2);
    CHECK(m.faces.size() == 8 * 2 + 8 * 2); // walls + two fans
    CHECK(m.verts.size() == 8 * 3 + 2);
    p.kind = PrimKind::Box;
    p.segX = 2;
    p.segY = 3;
    p.segZ = 1;
    m = makePrimitive(p, {0, 0, 0}, {10, 10, 10}, 2);
    CHECK(m.faces.size() == (size_t)(2 * (2 * 3 + 2 * 1 + 3 * 1)));
    p.kind = PrimKind::Plane;
    p.rows = 2;
    p.cols = 3;
    m = makePrimitive(p, {0, 0, 5}, {30, 20, 9}, 2);
    CHECK(m.faces.size() == 6 && faceNormal(m, 0).z > 0.99 && m.verts[0].z == 5);
    p.kind = PrimKind::Sphere;
    p.sides = 8;
    p.stacks = 4;
    m = makePrimitive(p, {0, 0, 0}, {10, 10, 10}, 2);
    CHECK(m.faces.size() == 8 * 4 && m.verts.size() == 8 * 3 + 2);
    p.kind = PrimKind::Geosphere;
    p.iterations = 1;
    m = makePrimitive(p, {0, 0, 0}, {10, 10, 10}, 2);
    CHECK(m.faces.size() == 80 && m.verts.size() == 42);
    // Smoothing: round walls in a group, caps flat.
    p.kind = PrimKind::Cylinder;
    m = makePrimitive(p, {0, 0, 0}, {10, 10, 10}, 2);
    int walls = 0, caps = 0;
    for (const Face& f : m.faces) (f.sg ? walls : caps)++;
    CHECK(walls == 8 * 2 && caps == 16);
    PrimKind k;
    CHECK(primKindFromName("torus", k) && k == PrimKind::Torus && !primKindFromName("teapot", k));

    // Rounded edges: 1-3 faces across every edge; still fills the box, closed, outward.
    for (int q = 1; q <= 3; ++q)
        for (int axis = 0; axis < 3; ++axis) {
            PrimParams rb;
            rb.kind = PrimKind::Box;
            rb.roundSegments = q;
            rb.roundSize = 40;
            const Mesh b = makePrimitive(rb, {0, 0, 0}, {64, 32, 48}, axis);
            CHECK(b.faces.size() == (size_t)(6 + 12 * q + 8 * q * q));
            Vec3 a, c;
            b.bounds(a, c);
            CHECK(closedAndConsistent(b) && signedVolume(b) > 0 && nearV(a, {0, 0, 0}, 0) && nearV(c, {64, 32, 48}, 0));
            rb.kind = PrimKind::Cylinder;
            rb.sides = 8;
            rb.segments = 2;
            const Mesh cy = makePrimitive(rb, {0, 0, 0}, {64, 32, 48}, axis);
            CHECK(cy.faces.size() == (size_t)((2 * q + 2) * 8 + 16));
            cy.bounds(a, c);
            CHECK(closedAndConsistent(cy) && signedVolume(cy) > 0 && nearV(a, {0, 0, 0}, 0) && nearV(c, {64, 32, 48}, 0));
        }
    // The rounding is the same size in cm on every edge: a 64 x 32 x 48 box, 40% of 16 =
    // 6.4 cm: the top face starts 6.4 cm in from each side.
    {
        PrimParams rb;
        rb.kind = PrimKind::Box;
        rb.roundSegments = 2;
        rb.roundSize = 40;
        const Mesh b = makePrimitive(rb, {0, 0, 0}, {64, 32, 48}, 2);
        double minX = 1e9;
        for (const Vec3& v : b.verts)
            if (v.z == 48) minX = std::min(minX, v.x);
        CHECK(near(minX, 6.4, 0.01));
    }

    // Rocks: same seed, same rock; another seed, another rock; flat bottom flat; about
    // the asked number of triangles; many seeds all valid.
    p = PrimParams();
    p.kind = PrimKind::Rock;
    p.rock.seed = 42;
    const Mesh r1 = makePrimitive(p, {0, 0, 0}, {60, 40, 30}, 2);
    const Mesh r2 = makePrimitive(p, {0, 0, 0}, {60, 40, 30}, 2);
    bool same = r1.verts.size() == r2.verts.size();
    for (size_t i = 0; same && i < r1.verts.size(); ++i) same = nearV(r1.verts[i], r2.verts[i], 0);
    CHECK(same);
    p.rock.seed = 43;
    const Mesh r3 = makePrimitive(p, {0, 0, 0}, {60, 40, 30}, 2);
    bool differs = r3.verts.size() != r1.verts.size();
    for (size_t i = 0; !differs && i < r1.verts.size(); ++i) differs = !nearV(r1.verts[i], r3.verts[i], 0);
    CHECK(differs);
    int bottomPts = 0;
    for (const Vec3& q : r1.verts) bottomPts += q.z == 0;
    CHECK(bottomPts >= 3);
    CHECK(std::abs((int)r1.faces.size() - 24) <= 4); // about the asked 24 triangles
    int bad = 0;
    for (unsigned seed = 1; seed <= 200; ++seed) {
        p.rock.seed = seed;
        p.rock.roughness = (seed * 37) % 101;
        p.rock.faces = 12 + (int)(seed % 49);
        p.rock.joinQuads = seed % 3 == 0;
        p.rock.flatBottom = seed % 4 != 0;
        const Mesh rk = makePrimitive(p, {-10, -10, 0}, {30, 20, 25}, (int)(seed % 3));
        Vec3 a, b;
        rk.bounds(a, b);
        bool flat = false;
        for (int f = 0; f < (int)rk.faces.size(); ++f) flat = flat || length(faceNormal(rk, f)) < 0.5;
        if (!closedAndConsistent(rk) || signedVolume(rk) <= 0 || flat || !nearV(a, {-10, -10, 0}, 0) ||
            !nearV(b, {30, 20, 25}, 0))
            ++bad;
    }
    CHECK(bad == 0);
    if (bad) std::fprintf(stderr, "  %d of 200 rocks invalid\n", bad);
}

// ---------------------------------------------------------------- points, edges, faces
#include "core/select_ops.hpp"

void testSelectOps() {
    using namespace mazz;
    Mesh box;
    addBox(box, {0, 0, 0}, {10, 10, 10});
    // Conversion.
    Selection s;
    s.mode = SelectMode::Piece;
    s.faces = {0, 1, 2, 3, 4, 5};
    CHECK(convertSelection(box, s, SelectMode::Vert).verts.size() == 8);
    CHECK(convertSelection(box, s, SelectMode::Edge).edges.size() == 12);
    CHECK(convertSelection(box, s, SelectMode::Face).faces.size() == 6);
    Selection top;
    top.mode = SelectMode::Vert;
    top.verts = {4, 5, 6, 7}; // z = 10
    Selection f = convertSelection(box, top, SelectMode::Face);
    CHECK(f.faces.size() == 1 && f.faces.count(1));
    CHECK(convertSelection(box, top, SelectMode::Edge).edges.size() == 4);
    CHECK(convertSelection(box, top, SelectMode::Piece).faces.size() == 6); // whole piece
    Selection oneEdge;
    oneEdge.mode = SelectMode::Edge;
    oneEdge.edges = {makeEdge(4, 5)};
    CHECK(convertSelection(box, oneEdge, SelectMode::Vert).verts == std::set<int>({4, 5}));
    CHECK(convertSelection(box, oneEdge, SelectMode::Face).faces.empty());

    // Picking in Top: a corner takes the two stacked points; faces nearest first.
    ViewGeom t;
    t.dir = ViewDir::Top;
    t.ortho.dir = ViewDir::Top;
    t.ortho.zoom = 10;
    t.w = t.h = 400;
    double sx, sy;
    t.project({10, 10, 0}, sx, sy);
    CHECK(pointsAt(box, t, sx + 2, sy - 1) == std::set<int>({3, 7}));
    t.project({5, 5, 0}, sx, sy);
    const std::vector<int> under = facesAt(box, t, sx, sy);
    CHECK(under.size() == 2 && under[0] == 1 && under[1] == 0); // top, then bottom
    t.project({5, 10, 0}, sx, sy);
    const std::vector<Edge> eu = edgesAt(box, t, sx, sy);
    CHECK(eu.size() >= 2 && eu[0] == makeEdge(6, 7)); // the top edge first
    // Rectangles: 2D sees through.
    double ax, ay, bx, by;
    t.project({-1, -1, 0}, ax, ay);
    t.project({11, 11, 0}, bx, by);
    CHECK(inRect(box, t, ax, ay, bx, by, SelectMode::Vert).verts.size() == 8);
    CHECK(inRect(box, t, ax, ay, bx, by, SelectMode::Face).faces.size() == 6);
    // 3D: only what can be seen (from above at an angle: one corner hidden).
    ViewGeom g;
    g.dir = ViewDir::Persp;
    g.w = 800;
    g.h = 600;
    g.cam.target = {5, 5, 5};
    g.cam.yaw = 30;
    g.cam.pitch = 30;
    g.cam.distance = 80;
    const Selection seen = inRect(box, g, 0, 0, 800, 600, SelectMode::Vert);
    CHECK(seen.verts.size() == 7);
    CHECK(inRect(box, g, 0, 0, 800, 600, SelectMode::Face).faces.size() == 3);
    int hidden = 0;
    for (int v = 0; v < 8; ++v) hidden += !pointVisible(box, g, v);
    CHECK(hidden == 1);

    // Grow / shrink on a 5x5 plane: the centre face grows to its 3x3 neighbourhood and
    // shrinks back (faces at the plane's own edge would stay: nothing unselected there).
    PrimParams pp;
    pp.kind = PrimKind::Plane;
    pp.rows = 5;
    pp.cols = 5;
    const Mesh plane = makePrimitive(pp, {0, 0, 0}, {50, 50, 0}, 2);
    Selection c;
    c.mode = SelectMode::Face;
    c.faces = {12}; // row 2, column 2
    const Selection grown = growSelection(plane, c);
    CHECK(grown.faces.size() == 9);
    CHECK(shrinkSelection(plane, grown).faces == c.faces);
    Selection pt;
    pt.mode = SelectMode::Vert;
    pt.verts = {14}; // an inner point (row 2, column 2 of the 6x6 points)
    CHECK(growSelection(plane, pt).verts.size() == 5);

    // Loops and rings on a cylinder: 8 sides, 3 segments; points row k: k*8 + i.
    pp = PrimParams();
    pp.kind = PrimKind::Cylinder;
    pp.sides = 8;
    pp.segments = 3;
    const Mesh cyl = makePrimitive(pp, {0, 0, 0}, {20, 20, 30}, 2);
    const std::set<Edge> loop = edgeLoop(cyl, makeEdge(8, 9));
    CHECK(loop.size() == 8);
    bool level = true;
    for (const Edge& e : loop) level = level && cyl.verts[e.first].z == 10 && cyl.verts[e.second].z == 10;
    CHECK(level);
    const std::set<Edge> ring = edgeRing(cyl, makeEdge(8, 9));
    CHECK(ring.size() == 4); // rows 0..3 between the same two columns
    CHECK(ring.count(makeEdge(0, 1)) && ring.count(makeEdge(24, 25)));
    const std::set<Edge> vring = edgeRing(cyl, makeEdge(8, 16)); // an upright edge: round the cylinder
    CHECK(vring.size() == 8);

    // Delete: faces using the selection.
    CHECK(facesToDelete(box, top).size() == 5); // all but the bottom
    CHECK(facesToDelete(box, oneEdge).size() == 2);
}
