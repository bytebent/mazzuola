#pragma once
// The maths behind the selection box (ARCHITECTURE §6 "Brush feel"): stretching points
// between a box's sides, turning them about a centre, copying faces. Pure functions;
// results are cleaned (1/1000 cm) by the MeshEdit that stores them.
#include "core/document.hpp"
#include <set>

namespace mazz {

// A point inside box `from` mapped to the same relative place in box `to`. Along an
// axis where `from` is flat, the point just moves with the box.
Vec3 stretchInBox(const Vec3& p, const Vec3& fromMn, const Vec3& fromMx, const Vec3& toMn, const Vec3& toMx);

// `p` turned by `degrees` about the line through `centre` along `axis` (counter-
// clockwise seen from where `axis` points). Exact at multiples of 90°.
Vec3 rotateAbout(const Vec3& p, const Vec3& centre, const Vec3& axis, double degrees);

// Angle snapping: to steps of `step` degrees (no snapping when step <= 0).
double snapAngle(double degrees, double step);

// Copies `faces` (with their own new points) inside the edit; returns the new faces.
std::set<int> copyFaces(MeshEdit& e, const std::set<int>& faces);

} // namespace mazz
