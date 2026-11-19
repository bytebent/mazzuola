#pragma once
// The rock generator (Create -> Rock): a random low-poly rock from a seed.
// Built from the convex hull of jittered points on an ellipsoid, optionally cut flat at
// the bottom, then roughened along the normals (kept small enough that the surface never
// crosses itself). Same seed and values -> same rock, on every platform.
#include "core/mesh.hpp"
#include "core/primitives.hpp"
#include <vector>

namespace mazz {

// Rock in unit space ([-0.5, 0.5] on each axis, exactly filling it).
Mesh makeRock(const PrimParams& p);

// Convex hull of points (triangles, wound outward). Points should be in general position;
// near-duplicates and points inside are ignored.
Mesh convexHull(const std::vector<Vec3>& pts);

// True if two non-neighbouring triangles of the mesh cross each other.
bool selfIntersects(const Mesh& m);

} // namespace mazz
