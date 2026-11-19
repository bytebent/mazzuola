#pragma once
// The rock generator (ARCHITECTURE §6 Rock): a random low-poly rock filling a box.
// Points scattered on a sphere (jittered more with roughness), their convex hull, then
// some points pushed inward along their normals (never so far that a face turns over),
// an optional flat bottom, optionally joined into quads. Same seed, same rock.
#include "core/mesh.hpp"

namespace mazz {

struct RockParams {
    int faces = 24;           // target triangle count, 12..60
    double roughness = 35;    // %, 0..100
    bool flatBottom = true;
    double bottomHeight = 15; // %, how much of the height is cut flat (0..60)
    bool joinQuads = false;
    unsigned seed = 1;
};

RockParams clampRock(const RockParams& p);

// The rock in local coordinates: u, v across (-1..1), w up (0..1), outward faces,
// round faces in smoothing group 1, the flat bottom flat.
Mesh makeRockLocal(const RockParams& p);

// Convex hull of points (triangles, outward). Points inside or on a face are dropped.
Mesh convexHull(const std::vector<Vec3>& pts);

} // namespace mazz
