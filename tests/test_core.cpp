// Core tests: plain asserts, no framework. Run: ./build/mazz_core_tests
#include "core/camera.hpp"
#include "core/grid.hpp"
#include "core/grid_lines.hpp"
#include "check.hpp"
#include <cmath>
#include <cstdio>
#include <string>

using namespace mazz;

void testModel(); // test_model.cpp

static void testGrid() {
    Grid g;
    CHECK(g.size == 8);
    g.doubleUp();
    CHECK(g.size == 16);
    for (int i = 0; i < 20; ++i) g.doubleUp();
    CHECK(g.size == Grid::kMax);
    for (int i = 0; i < 30; ++i) g.halve();
    CHECK(g.size == Grid::kMin);
    g.setSize(1000);
    CHECK(g.size == Grid::kMax);
    g.setSize(5);
    CHECK(g.snapValue(12) == 10);
    CHECK(g.snapValue(13) == 15);
    g.snap = false;
    CHECK(g.snapValue(13) == 13);
    g.setSize(1);
    CHECK(g.drawnStep(10) == 1);   // 10 px apart: drawn as is
    CHECK(g.drawnStep(1) == 8);    // 1 px apart: doubled to 8 px
}

static void testOrthoAxes() {
    // Every direction: right x up points toward the viewer (opposite to forward), axes
    // are unit and perpendicular.
    for (int d = 0; d < kViewDirCount; ++d) {
        if ((ViewDir)d == ViewDir::Persp) continue;
        const OrthoAxes a = orthoAxes((ViewDir)d);
        CHECK(a.u != a.v);
        CHECK(near(dot(a.right(), a.up()), 0));
        CHECK(near(length(a.forward()), 1));
        CHECK(nearV(cross(a.right(), a.up()), -a.forward()));
    }
    CHECK(nearV(orthoAxes(ViewDir::Top).forward(), {0, 0, -1}));
    CHECK(nearV(orthoAxes(ViewDir::Bottom).forward(), {0, 0, 1}));
    CHECK(nearV(orthoAxes(ViewDir::Front).forward(), {0, 1, 0}));
    CHECK(nearV(orthoAxes(ViewDir::Back).forward(), {0, -1, 0}));
    CHECK(nearV(orthoAxes(ViewDir::Right).forward(), {-1, 0, 0}));
    CHECK(nearV(orthoAxes(ViewDir::Left).forward(), {1, 0, 0}));
    ViewDir d;
    CHECK(viewDirFromName("front", d) && d == ViewDir::Front);
    CHECK(viewDirFromName("3d", d) && d == ViewDir::Persp);
    CHECK(!viewDirFromName("sideways", d));
}

static void testOrthoCamera() {
    OrthoCamera c;
    c.dir = ViewDir::Front;
    c.cu = 10;
    c.cv = -4;
    c.zoom = 3;
    const double w = 400, h = 300;
    // Screen <-> plane round trip.
    double u, v, px, py;
    c.screenToPlane(123, 45, w, h, u, v);
    c.planeToScreen(u, v, w, h, px, py);
    CHECK(near(px, 123) && near(py, 45));
    // Centre of the screen is the camera centre; up on screen is +v.
    c.screenToPlane(w / 2, h / 2, w, h, u, v);
    CHECK(near(u, 10) && near(v, -4));
    double u2, v2;
    c.screenToPlane(w / 2, h / 2 - 30, w, h, u2, v2);
    CHECK(near(v2 - v, 10)); // 30 px / 3 px per cm
    // Plane <-> world.
    const Vec3 p = c.planeToWorld(7, 9);
    CHECK(nearV(p, {7, 0, 9}));
    c.worldToPlane({7, 55, 9}, u, v);
    CHECK(near(u, 7) && near(v, 9));
    // Zoom keeps the point under the cursor.
    c.screenToPlane(300, 50, w, h, u, v);
    c.zoomAt(1.7, 300, 50, w, h);
    c.screenToPlane(300, 50, w, h, u2, v2);
    CHECK(near(u, u2, 1e-9) && near(v, v2, 1e-9));
    // Pan: dragging right by 30 px moves the content right (centre moves left).
    const double cu0 = c.cu;
    c.pan(30, 0);
    CHECK(near(c.cu, cu0 - 30 / c.zoom));
    // The matrices put the centre in the middle of the screen and +u to the right.
    const Mat4 vp = c.projection(w, h) * c.view();
    const Vec3 ndc = transformPoint(vp, c.planeToWorld(c.cu, c.cv));
    CHECK(near(ndc.x, 0, 1e-4) && near(ndc.y, 0, 1e-4));
    const Vec3 ndcR = transformPoint(vp, c.planeToWorld(c.cu + 10, c.cv + 10));
    CHECK(ndcR.x > 0 && ndcR.y > 0);
    CHECK(near(ndcR.x, 10 * c.zoom / (w / 2), 1e-4));
    // Every direction maps its own right/up to +x/+y on screen.
    for (int d = 0; d < kViewDirCount; ++d) {
        if ((ViewDir)d == ViewDir::Persp) continue;
        OrthoCamera o;
        o.dir = (ViewDir)d;
        const Mat4 m = o.projection(w, h) * o.view();
        const OrthoAxes a = orthoAxes(o.dir);
        const Vec3 r = transformPoint(m, a.right() * 10), up = transformPoint(m, a.up() * 10);
        CHECK(r.x > 0 && near(r.y, 0, 1e-5));
        CHECK(up.y > 0 && near(up.x, 0, 1e-5));
    }
}

static void testCamera3D() {
    Camera3D c;
    c.yaw = 0;
    c.pitch = 0;
    c.distance = 100;
    CHECK(nearV(c.forward(), {0, 1, 0}));   // yaw 0: looking along +Y, like the Front view
    CHECK(nearV(c.eye(), {0, -100, 0}));
    CHECK(nearV(c.right(), {1, 0, 0}));
    CHECK(nearV(c.up(), {0, 0, 1}));
    c.pitch = 30;
    CHECK(c.eye().z > 0);                    // positive pitch: above, looking down
    c.orbit(0, 100);
    CHECK(c.pitch == 89);                    // clamped
    // The target lands in the middle of the screen.
    c = Camera3D{};
    const Mat4 vp = c.projection(1.5) * c.view();
    const Vec3 ndc = transformPoint(vp, c.target);
    CHECK(near(ndc.x, 0, 1e-4) && near(ndc.y, 0, 1e-4));
    // Look keeps the eye in place; fly moves eye and target together.
    const Vec3 eye0 = c.eye();
    c.look(20, -10);
    CHECK(nearV(c.eye(), eye0, 1e-6));
    const Vec3 t0 = c.target;
    c.fly(10, 0, 5);
    CHECK(nearV(c.target - t0, c.forward() * 10 + Vec3{0, 0, 5}, 1e-9));
    // Dolly is clamped.
    c.dolly(1e-9);
    CHECK(c.distance == Camera3D::kMinDistance);
    // Panning moves the target sideways, not along the view.
    c = Camera3D{};
    const Vec3 tA = c.target;
    c.panPx(50, 0, 500);
    CHECK(near(dot(c.target - tA, c.forward()), 0, 1e-9));
    CHECK(dot(c.target - tA, c.right()) < 0); // drag right: the scene follows the mouse
}

static void testGridLines() {
    Grid g;
    g.setSize(10);
    OrthoCamera c;
    c.zoom = 1; // 10 px apart
    const auto lines = gridLines2D(g, c, 200, 100); // u -100..100, v -50..50
    int vertical = 0, horizontal = 0, axisU = 0, axisV = 0, major = 0;
    for (const GridLine2D& l : lines) {
        if (l.kind == GridLineKind::AxisU) ++axisU;
        else if (l.kind == GridLineKind::AxisV) ++axisV;
        else (l.vertical ? vertical : horizontal) += 1;
        if (l.kind == GridLineKind::Major) {
            ++major;
            CHECK(std::fmod(std::fabs(l.value), 80.0) < 1e-9);
        }
    }
    CHECK(axisU == 1 && axisV == 1);
    CHECK(vertical == 20);   // -100..100 step 10 without 0
    CHECK(horizontal == 10); // -50..50 step 10 without 0
    CHECK(major == 2);       // u = -80, 80
    // Hidden grid: axes only.
    g.visible = false;
    CHECK(gridLines2D(g, c, 200, 100).size() == 2);
    // 3D: always the two axes, and lines only on the ground plane.
    g.visible = true;
    Camera3D cam;
    double step = 0, extent = 0;
    const auto l3 = gridLines3D(g, cam, step, extent);
    CHECK(step >= g.size);
    CHECK(l3.size() > 2);
    for (const GridLine3D& l : l3) CHECK(l.a.z == 0 && l.b.z == 0);
}

int main() {
    testGrid();
    testOrthoAxes();
    testOrthoCamera();
    testCamera3D();
    testGridLines();
    testModel();
    if (gFailures) {
        std::fprintf(stderr, "%d of %d checks failed\n", gFailures, gChecks);
        return 1;
    }
    std::printf("all %d checks passed\n", gChecks);
    return 0;
}
