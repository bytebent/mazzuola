// Milestone 1 core tests: mesh, primitives, rock, selection, editing, picking, undo.
#include "check.hpp"
#include "core/document.hpp"
#include "core/ops.hpp"
#include "core/pick.hpp"
#include "core/primitives.hpp"
#include "core/rock.hpp"
#include <cstdio>

using namespace mazz;

namespace {

bool fitsBox(const Mesh& m, const Vec3& centre, const Vec3& size, double tol = 1e-6) {
    Vec3 mn, mx;
    if (!m.bounds(mn, mx)) return false;
    for (int a = 0; a < 3; ++a) {
        if (std::fabs(mn[a] - (centre[a] - size[a] / 2)) > tol * std::max(1.0, size[a])) return false;
        if (std::fabs(mx[a] - (centre[a] + size[a] / 2)) > tol * std::max(1.0, size[a])) return false;
    }
    return true;
}

void testPrimitives() {
    for (int k = 0; k < kPrimKindCount; ++k) {
        for (int axis = 0; axis < 3; ++axis) {
            PrimParams p;
            p.kind = (PrimKind)k;
            p.axis = axis;
            p.centre = {10, -20, 5};
            p.size = {80, 60, 40};
            if (p.kind == PrimKind::Torus) p.size[axis] = 15; // thickness along its axis
            const Mesh m = makePrimitive(p);
            const bool ok = meshOk(m);
            CHECK(ok);
            if (!ok) std::fprintf(stderr, "  %s axis %d\n", primKindName(p.kind), axis);
            if (p.kind == PrimKind::Plane) {
                CHECK(m.faces.size() == 1);
                // Flat, lying across the axis.
                Vec3 mn, mx;
                m.bounds(mn, mx);
                CHECK(near(mn[axis], mx[axis]));
                continue;
            }
            const bool closed = isClosed(m);
            CHECK(closed);
            if (!closed) std::fprintf(stderr, "  not closed: %s axis %d\n", primKindName(p.kind), axis);
            CHECK(signedVolume(m) > 0); // faces point outward
            // Round shapes fit their box exactly along their main directions.
            Vec3 size = p.size;
            // (A torus reaches its full thickness only if a ring of points lands on top.)
            if (p.kind == PrimKind::Torus) {
                Vec3 mn, mx;
                m.bounds(mn, mx);
                for (int a = 0; a < 3; ++a)
                    if (a != axis) CHECK(near(mx[a] - mn[a], size[a], 1e-6));
            }
            const bool fit = p.kind == PrimKind::Box || p.kind == PrimKind::Rock ||
                             p.kind == PrimKind::Cylinder || p.kind == PrimKind::Tube || p.kind == PrimKind::Cone;
            if (fit) {
                const bool f = fitsBox(m, p.centre, size, 1e-6);
                CHECK(f);
                if (!f) std::fprintf(stderr, "  doesn't fit its box: %s axis %d\n", primKindName(p.kind), axis);
            }
        }
    }
    // Counts and options.
    PrimParams p;
    p.kind = PrimKind::Box;
    p.segX = 2;
    p.segY = 3;
    p.segZ = 1;
    Mesh b = makePrimitive(p);
    CHECK(b.faces.size() == (size_t)(2 * (2 * 3 + 3 * 1 + 2 * 1)));
    CHECK(isClosed(b));
    p.round = 8;
    p.roundSegs = 3;
    b = makePrimitive(p);
    CHECK(meshOk(b) && isClosed(b) && signedVolume(b) > 0);
    CHECK(fitsBox(b, p.centre, p.size, 1e-9));
    for (const Face& f : b.faces) CHECK(f.sg == 1);
    p.round = 1000; // fully round: still a valid closed shape
    b = makePrimitive(p);
    CHECK(meshOk(b) && isClosed(b));

    p = PrimParams{};
    p.kind = PrimKind::Cylinder;
    p.sides = 6;
    p.stacks = 2;
    p.capRings = 2;
    Mesh c = makePrimitive(p);
    // sides*stacks quads + per cap: sides quads (one inner ring) + sides triangles
    CHECK(c.faces.size() == (size_t)(6 * 2 + 2 * (6 + 6)));
    CHECK(c.triangleCount() == 2 * 6 * 2 + 2 * (2 * 6 + 6));

    p = PrimParams{};
    p.kind = PrimKind::Sphere;
    p.sides = 8;
    p.stacks = 4;
    Mesh s = makePrimitive(p);
    CHECK(s.verts.size() == (size_t)(2 + 8 * 3));
    p.kind = PrimKind::Geosphere;
    p.iterations = 2;
    s = makePrimitive(p);
    CHECK(s.faces.size() == 20u * 16u);
    for (const Vec3& v : s.verts) {
        // On the ellipsoid filling the box (a sphere here: all sizes 64).
        CHECK(near(length(v), 32, 1e-9));
    }
    p = PrimParams{};
    p.kind = PrimKind::Plane;
    p.rows = 2;
    p.cols = 3;
    p.flipDiagonals = true;
    Mesh pl = makePrimitive(p);
    CHECK(pl.faces.size() == 6 && pl.faces[0].diag);
    CHECK(near(pl.faceNormal(0).z, 1));
    // Clamping.
    p.rows = 0;
    p.sides = 1;
    CHECK(clampParams(p).rows == 1 && clampParams(p).sides == 3);
    PrimKind kind;
    CHECK(primKindFromName("geosphere", kind) && kind == PrimKind::Geosphere);
}

void testRock() {
    PrimParams p;
    p.kind = PrimKind::Rock;
    p.size = {100, 80, 60};
    int bad = 0, offCount = 0;
    for (uint32_t seed = 1; seed <= 1000; ++seed) {
        p.seed = seed;
        p.faces = 12 + (int)(seed % 49);
        p.flatBottom = seed % 3 != 0;
        p.roughness = (seed * 7) % 101;
        const Mesh m = makePrimitive(p);
        const bool ok = meshOk(m) && isClosed(m) && signedVolume(m) > 0 && !selfIntersects(m) &&
                        fitsBox(m, p.centre, p.size, 1e-9);
        if (!ok) {
            if (bad < 5) std::fprintf(stderr, "  rock seed %u failed\n", seed);
            ++bad;
        }
        const double t = m.triangleCount();
        if (std::fabs(t - p.faces) > 0.2 * p.faces) {
            if (offCount < 5) std::fprintf(stderr, "  rock seed %u: %d triangles for %d\n", seed, (int)t, p.faces);
            ++offCount;
        }
        if (p.flatBottom && seed % 10 == 1) {
            // Flat bottom: the lowest points are all at the same height (the floor).
            Vec3 mn, mx;
            m.bounds(mn, mx);
            int onFloor = 0;
            for (const Vec3& v : m.verts)
                if (near(v.z, mn.z, 1e-9)) ++onFloor;
            CHECK(onFloor >= 3);
        }
    }
    CHECK(bad == 0);
    CHECK(offCount == 0);
    // Same seed, same rock.
    p.seed = 42;
    const Mesh a = makePrimitive(p), b = makePrimitive(p);
    CHECK(a.verts.size() == b.verts.size() && a.faces.size() == b.faces.size());
    bool same = a.verts.size() == b.verts.size();
    for (size_t i = 0; same && i < a.verts.size(); ++i) same = a.verts[i].x == b.verts[i].x && a.verts[i].z == b.verts[i].z;
    CHECK(same);
    p.seed = 43;
    const Mesh c = makePrimitive(p);
    CHECK(c.verts.size() != a.verts.size() || c.verts[0].x != a.verts[0].x);
    // Join into quads makes quads, still valid.
    p.joinQuads = true;
    p.roughness = 0;
    p.seed = 7;
    const Mesh q = makePrimitive(p);
    CHECK(meshOk(q) && isClosed(q));
    // Smooth off: flat.
    p.smooth = false;
    for (const Face& f : makePrimitive(p).faces) CHECK(f.sg == 0);
}

Mesh unitCube() {
    PrimParams p;
    p.kind = PrimKind::Box;
    p.size = {2, 2, 2};
    return makePrimitive(p);
}

void testSelection() {
    PrimParams p;
    p.kind = PrimKind::Cylinder;
    p.sides = 8;
    p.stacks = 3;
    Mesh m = makePrimitive(p);
    // Pieces: one; two after appending a box.
    Mesh two = m;
    two.append(unitCube());
    const std::vector<int> ids = pieceIds(two);
    CHECK(ids.front() != ids.back());
    CHECK(pieceOf(two, 0).size() == m.faces.size());
    // Edge loop around the side: a ring of 8 horizontal edges at stack 1.
    // Find a horizontal edge between two side points at the same height (not on a cap).
    Edge horiz{-1, -1};
    for (const Edge& e : m.edges()) {
        const Vec3 a = m.verts[e.first], b = m.verts[e.second];
        if (near(a.z, b.z) && a.z > -31 && a.z < 31 && near(length(Vec3{a.x, a.y, 0}), 32, 1e-6) &&
            near(length(Vec3{b.x, b.y, 0}), 32, 1e-6)) { horiz = e; break; }
    }
    CHECK(horiz.first >= 0);
    CHECK(edgeLoop(m, horiz).size() == 8);
    // Edge ring from a vertical side edge: around the side (8 edges).
    Edge vert{-1, -1};
    for (const Edge& e : m.edges()) {
        const Vec3 a = m.verts[e.first], b = m.verts[e.second];
        if (near(a.x, b.x) && near(a.y, b.y) && !near(a.z, b.z)) { vert = e; break; }
    }
    CHECK(vert.first >= 0);
    CHECK(edgeRing(m, vert).size() == 8);
    // Ring from the horizontal edge: up the side, stops at the caps' triangles... the
    // side has 3 stacks: 4 horizontal edges in that column, then the cap quads continue.
    CHECK(edgeRing(m, horiz).size() >= 4);

    // Grow / shrink faces on the cube.
    Mesh cube = unitCube();
    Selection s;
    s.faces = {0};
    growSelection(cube, s, SelectMode::Face);
    CHECK(s.faces.size() == 5); // every face but the opposite one shares a point
    shrinkSelection(cube, s, SelectMode::Face);
    CHECK(s.faces.empty() || s.faces.size() <= 1);
    // Convert: faces -> vertices -> faces.
    s.clear();
    s.faces = {0, 1};
    Selection v = convertSelection(cube, s, SelectMode::Face, SelectMode::Vertex);
    CHECK(v.verts.size() == 6 || v.verts.size() == 8); // neighbours share an edge; opposite faces don't
    Selection back = convertSelection(cube, v, SelectMode::Vertex, SelectMode::Face);
    CHECK(back.faces.count(0) && back.faces.count(1));
    Selection piece = convertSelection(cube, s, SelectMode::Face, SelectMode::Piece);
    CHECK(piece.faces.size() == 6);
    // Similar: direction.
    const std::set<int> up = similarFaces(cube, {0}, Similar::Direction);
    CHECK(up.size() == 1);
    CHECK(similarFaces(cube, {0}, Similar::SmoothingGroup).size() == 6);
}

void testEditing() {
    // Transforms.
    const Affine r = rotationXYZ({0, 0, 90});
    CHECK(nearV(r.apply({1, 0, 0}), {0, 1, 0}));
    CHECK(r.m[0][0] == 0 && r.m[1][0] == 1); // exact for right angles
    const Affine a = about(scaling({2, 2, 2}), {1, 1, 1});
    CHECK(nearV(a.apply({1, 1, 1}), {1, 1, 1}));
    CHECK(nearV(a.apply({2, 1, 1}), {3, 1, 1}));
    CHECK(mirrorAxis(0, {5, 0, 0}).det() < 0);

    // Mirror keeps faces pointing outward (winding flipped).
    Mesh cube = unitCube();
    std::set<int> all;
    for (int f = 0; f < (int)cube.faces.size(); ++f) all.insert(f);
    mirrorFaces(cube, all, 0, {10, 0, 0}, false);
    CHECK(isClosed(cube) && signedVolume(cube) > 0);
    Vec3 mn, mx;
    cube.bounds(mn, mx);
    CHECK(near(mn.x, 19) && near(mx.x, 21));
    // Mirror copy: two pieces, both outward.
    Mesh mc = unitCube();
    mirrorFaces(mc, all, 0, {5, 0, 0}, true);
    CHECK(mc.faces.size() == 12 && signedVolume(mc) > 0 && meshOk(mc));
    // Transform with a mirroring scale also keeps faces outward.
    Mesh tc = unitCube();
    std::set<int> verts;
    for (int v = 0; v < (int)tc.verts.size(); ++v) verts.insert(v);
    transformPoints(tc, verts, scaling({-1, 1, 1}));
    CHECK(signedVolume(tc) > 0);

    // Clone in a row: 3 copies 10 cm apart.
    Mesh cl = unitCube();
    CloneParams cp;
    cp.count = 3;
    cp.offset = {10, 0, 0};
    const std::set<int> copies = cloneFaces(cl, all, cp);
    CHECK(copies.size() == 18 && cl.faces.size() == 24);
    cl.bounds(mn, mx);
    CHECK(near(mx.x, 31));
    // Total offset spread over the copies.
    Mesh ct = unitCube();
    cp.offsetIsTotal = true;
    cp.offset = {30, 0, 0};
    cloneFaces(ct, all, cp);
    ct.bounds(mn, mx);
    CHECK(near(mx.x, 31));
    // Circle: 4 copies rotated 90 degrees about a pivot.
    Mesh cc = unitCube();
    CloneParams circle;
    circle.count = 3;
    circle.rotate = {0, 0, 90};
    circle.pivot = {10, 0, 0};
    cloneFaces(cc, all, circle);
    cc.bounds(mn, mx);
    CHECK(near(mn.x, -1) && near(mx.x, 21) && near(mn.y, -11) && near(mx.y, 11));
    // Scale per copy with "scale the offset": geometric spacing.
    Mesh cs = unitCube();
    CloneParams sc;
    sc.count = 2;
    sc.offset = {10, 0, 0};
    sc.scale = {50, 50, 50};
    sc.scaleOffset = true;
    sc.pivot = {0, 0, 0};
    const std::set<int> sCopies = cloneFaces(cs, all, sc);
    // Copy 2: offset 10 + 5 = 15, scale 0.25 (half size 0.25).
    Vec3 c2;
    int n2 = 0;
    for (int f : sCopies)
        if (f >= 12) { c2 += cs.faceCentre(f); ++n2; }
    CHECK(n2 == 6 && near((c2 / n2).x, 15));

    // Flatten and snap.
    Mesh fl = unitCube();
    flattenPoints(fl, verts, 2, 5);
    for (const Vec3& v : fl.verts) CHECK(v.z == 5);
    Mesh sn = unitCube();
    transformPoints(sn, verts, translation({0.3, 0, 0}));
    snapPoints(sn, verts, 1);
    for (const Vec3& v : sn.verts) CHECK(v.x == std::round(v.x));

    // Delete: selecting one corner removes the three faces using it.
    Mesh dl = unitCube();
    Selection s;
    s.verts = {0};
    CHECK(deleteSelection(dl, s, SelectMode::Vertex) == 3);
    CHECK(dl.faces.size() == 3 && meshOk(dl));
    // Hide.
    Mesh hd = unitCube();
    setHidden(hd, {0, 1}, true);
    CHECK(hd.triangleCount(false) == 8);

    // Move along normal: the top face of a cube moves up.
    Mesh mv = unitCube();
    int top = -1;
    for (int f = 0; f < (int)mv.faces.size(); ++f)
        if (mv.faceNormal(f).z > 0.9) top = f;
    moveAlongNormal(mv, {top}, 3);
    mv.bounds(mn, mx);
    CHECK(near(mx.z, 4));

    // Join triangles into quads: a triangulated plane becomes quads again.
    Mesh tri;
    tri.addVertex({0, 0, 0});
    tri.addVertex({1, 0, 0});
    tri.addVertex({1, 1, 0});
    tri.addVertex({0, 1, 0});
    tri.addTri(0, 1, 2);
    tri.addTri(0, 2, 3);
    CHECK(joinTrianglesIntoQuads(tri, 5) == 1);
    CHECK(tri.faces.size() == 1 && tri.faces[0].n == 4 && near(tri.faceNormal(0).z, 1));
    CHECK(near(tri.faceArea(0), 1));
    // Not across a fold.
    Mesh fold;
    fold.addVertex({0, 0, 0});
    fold.addVertex({1, 0, 0});
    fold.addVertex({1, 1, 1});
    fold.addVertex({0, 1, 0});
    fold.addTri(0, 1, 2);
    fold.addTri(0, 2, 3);
    CHECK(joinTrianglesIntoQuads(fold, 5) == 0);

    // Normals: flat cube corners match faces; a smooth sphere's normals point outward.
    const Mesh c = unitCube();
    const auto cn = cornerNormals(c);
    for (int f = 0; f < (int)c.faces.size(); ++f) CHECK(nearV(cn[f][0], c.faceNormal(f)));
    PrimParams sp;
    sp.kind = PrimKind::Sphere;
    const Mesh sph = makePrimitive(sp);
    const auto sn2 = cornerNormals(sph);
    bool outward = true;
    for (int f = 0; f < (int)sph.faces.size(); ++f)
        for (int k = 0; k < sph.faces[f].n; ++k)
            outward = outward && dot(sn2[f][k], normalize(sph.verts[sph.faces[f].v[k]])) > 0.95;
    CHECK(outward);
}

void testPicking() {
    // A 2x2x2 cube at the origin seen from the front (looking along +Y).
    Mesh cube = unitCube();
    OrthoCamera cam;
    cam.dir = ViewDir::Front;
    cam.zoom = 10; // 10 px per cm
    const double w = 200, h = 200;
    Projector pr;
    pr.viewProj = cam.projection(w, h) * cam.view();
    pr.w = w;
    pr.h = h;
    double x, y, d;
    CHECK(pr.project({1, 0, 1}, x, y, d) && near(x, 110, 1e-3) && near(y, 90, 1e-3));
    // The front face (at y = -1) is hit first.
    const FaceHit hit = pickFace(cube, pr, 100, 100);
    CHECK(hit.face >= 0 && near(cube.faceNormal(hit.face).y, -1));
    CHECK(pickFace(cube, pr, 10, 10).face < 0);
    // Points: nearest within the radius; 2D sees through.
    const int v = pickVertex(cube, pr, 111, 89, 4);
    CHECK(v >= 0 && near(cube.verts[v].x, 1) && near(cube.verts[v].z, 1));
    CHECK(pickVertex(cube, pr, 150, 150, 4) < 0);
    Edge e;
    CHECK(pickEdge(cube, pr, 110, 100, 3, e)); // the vertical edge at x = 1
    CHECK(near(cube.verts[e.first].x, 1) && near(cube.verts[e.second].x, 1));
    // Box select: everything (the whole cube is inside).
    Selection s = boxSelect(cube, pr, 0, 0, 200, 200, SelectMode::Face);
    CHECK(s.faces.size() == 6);
    s = boxSelect(cube, pr, 105, 0, 200, 200, SelectMode::Vertex); // right half: x = 1 points
    CHECK(s.verts.size() == 4);

    // 3D: points behind the cube can't be picked; front ones can.
    Camera3D c3;
    c3.yaw = 0;
    c3.pitch = 0;
    c3.distance = 20;
    Projector p3;
    p3.viewProj = c3.projection(1.0) * c3.view();
    p3.w = p3.h = 400;
    p3.perspective = true;
    CHECK(pickFace(cube, p3, 200, 200).face >= 0);
    // The front-top-right corner (1,-1,1) is visible; the back one (1,1,1) is hidden.
    p3.project({1, -1, 1}, x, y, d);
    const int front = pickVertex(cube, p3, x, y, 3);
    CHECK(front >= 0 && near(cube.verts[front].y, -1));
    p3.project({1, 1, 1}, x, y, d);
    const int back = pickVertex(cube, p3, x, y, 2);
    CHECK(back < 0 || near(cube.verts[back].y, -1));
}

void testDocument() {
    Document doc;
    CHECK(!doc.dirty && !doc.canUndo());
    doc.change("Add box", [](Model& m, Selection& s) {
        m.mesh = unitCube();
        s.faces = {0};
        return true;
    });
    CHECK(doc.dirty && doc.canUndo() && doc.undoName() == "Add box");
    CHECK(!doc.change("Nothing", [](Model&, Selection&) { return false; }));
    doc.markSaved();
    CHECK(!doc.dirty);
    doc.change("Move", [](Model& m, Selection& s) {
        transformPoints(m.mesh, selectedVerts(m.mesh, s, SelectMode::Face), translation({5, 0, 0}));
        return true;
    });
    CHECK(doc.dirty);
    CHECK(doc.undo());
    CHECK(!doc.dirty); // back at the saved state
    CHECK(doc.sel.faces.count(0));
    CHECK(doc.redo() && doc.dirty);
    CHECK(doc.undo() && doc.undo());
    CHECK(doc.model.mesh.faces.empty() && doc.sel.empty() && doc.dirty);
    // A change after undo drops the redo steps.
    doc.change("Other", [](Model& m, Selection&) { m.pivot = {1, 2, 3}; return true; });
    CHECK(!doc.canRedo());
    // Selection is cleaned against the result.
    doc.change("Hide", [](Model& m, Selection& s) {
        m.mesh = unitCube();
        s.faces = {0, 1};
        m.mesh.faces[1].hidden = true;
        return true;
    });
    CHECK(doc.sel.faces.size() == 1);
}

} // namespace

void runM1Tests() {
    testPrimitives();
    testRock();
    testSelection();
    testEditing();
    testPicking();
    testDocument();
}
