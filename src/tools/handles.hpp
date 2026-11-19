#pragma once
// The selection box's handles, shared by every tool that shows a box (ARCHITECTURE §4,
// §6 "Brush feel"): in 2D views a rectangle with handles on its corners and sides
// (drag inside moves, a side handle pushes that side, Option+drag a corner rotates);
// in the 3D view, X/Y/Z rotation rings around the box's centre.
#include "core/grid.hpp"
#include "core/mesh.hpp"
#include <set>
#include "core/view_geom.hpp"
#include "imgui.h"

namespace mazz {

struct BoxGrab {
    enum Kind { None, Inside, Handle, Rotate, Ring } kind = None;
    int su = 0, sv = 0; // Handle/Rotate: which side on screen (-1 left/bottom, +1 right/top, 0 middle)
    int ringAxis = -1;  // Ring: 0/1/2 = x/y/z
};

// 2D views: what is under the cursor. `alt` turns corner handles into rotation.
BoxGrab hitBox2D(const ViewGeom& g, const Vec3& mn, const Vec3& mx, double sx, double sy, bool alt);
// 3D view: the rotation ring under the cursor (kind Ring), or None.
BoxGrab hitRings(const ViewGeom& g, const Vec3& mn, const Vec3& mx, double sx, double sy);

// Resizing: the side(s) `su`/`sv` of box mn0..mx0 moved by (du, dv) plane units in view
// `g`; the opposite sides stay. Sides land on the grid when snapping; never thinner
// than one grid step (or 1/1000 cm without snapping).
void resizeBox(const ViewGeom& g, const Grid& grid, const Vec3& mn0, const Vec3& mx0, int su, int sv, double du,
               double dv, Vec3& mn, Vec3& mx);
// Moving: a world offset for a drag of (du, dv) plane units in view `g`. When snapping,
// the box's lowest corner `mn0` lands on the grid (as in Scalpellino and Hammer), so
// something off the grid snaps onto it.
Vec3 moveOffset(const ViewGeom& g, const Grid& grid, const Vec3& mn0, double du, double dv);
// Rotation by dragging a corner (2D) or a ring (3D): the angle in degrees from the
// press point to the current point, around the box centre; and the world axis.
double dragAngle(const ViewGeom& g, const Vec3& centre, double x0, double y0, double x1, double y1, const Vec3& axis);
Vec3 viewAxisTowardViewer(const ViewGeom& g); // 2D views: the axis rotations turn about

// Drawing.
void drawBox2D(ImDrawList* dl, const ViewGeom& g, const Vec3& mn, const Vec3& mx, ImU32 col, bool handles, bool dashed);
void drawBox3D(ImDrawList* dl, const ViewGeom& g, const Vec3& mn, const Vec3& mx, ImU32 col);
void drawRings(ImDrawList* dl, const ViewGeom& g, const Vec3& mn, const Vec3& mx, int hotAxis);
// Arrows: X/Y/Z arrows at the selection's centre, in the 3D view only (all three,
// unless one points almost straight at the viewer). hitArrows gives the
// axis under the cursor or -1; arrowOffset the world offset along that axis for a drag
// from (x0, y0) to (x1, y1), snapped so the box's lowest corner lands on the grid.
int hitArrows(const ViewGeom& g, const Vec3& centre, double sx, double sy);
void drawArrows(ImDrawList* dl, const ViewGeom& g, const Vec3& centre, int hotAxis);
// Tests: a screen point on an arrow, `cm` further along the axis from where the arrow
// can be grabbed (false if that arrow isn't shown).
bool arrowPoint(const ViewGeom& g, const Vec3& centre, int axis, double cm, double& sx, double& sy);
Vec3 arrowOffset(const ViewGeom& g, const Grid& grid, const Vec3& centre, const Vec3& mn0, int axis, double x0,
                 double y0, double x1, double y1);

// Point mode: every point as a small square, the selected ones pink. In the 3D view only
// points on a face turned toward the camera.
void drawPoints(ImDrawList* dl, const ViewGeom& g, const Mesh& m, const std::set<int>& selected);

} // namespace mazz
