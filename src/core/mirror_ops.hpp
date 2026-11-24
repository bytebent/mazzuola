#pragma once
// v0.2.7 Mirror mode (ARCHITECTURE §6 Mirror mode, agreed 2026-11-22). The mirror plane
// is x, y or z = 0 (`axis` 0, 1, 2). Points pair up with their twins by position:
// selecting adds the twins, and after a change made on one side the fix-up puts each
// twin at the mirror of its point, so moves, turns and scales come out mirrored.
#include "core/document.hpp"
#include "core/selection.hpp"
#include <string>
#include <vector>

namespace mazz {

constexpr double kOnPlane = 1e-3; // cm: a point this close to the plane is on it

Vec3 mirrored(const Vec3& p, int axis);

// Each point's twin: the point at its mirrored position (-1: none). A point on the
// plane is its own twin. Points sharing a spot pair up in order.
std::vector<int> twinPoints(const Mesh& m, int axis);
// Points off the plane with no twin.
int unpairedPoints(const Mesh& m, int axis);
// Each face's twin: the face on the twins of its points (-1: none; a face across the
// plane can be its own).
std::vector<int> twinFaces(const Mesh& m, const std::vector<int>& twins);
// The selection with every twin added (points, edges, faces).
Selection withTwins(const Mesh& m, const Selection& s, int axis);

// After a change made on `side` (+1 or -1) of the plane: each changed point on that
// side puts its twin at its mirror; points that were on the plane are kept on it.
// `twins` are from before the change; points added by it are left alone.
void mirrorFix(MeshEdit& e, int axis, int side, const std::vector<int>& twins);

// Rebuilds the model symmetric: the half on `keepSide` (+1 / -1) stays, the other is
// made its mirror. Faces across the plane are cut along it first; faces lying in it go
// (they would be inside); the halves are welded along the plane. False with a reason
// when it can't (nothing on that side; a face crossing the plane twice; a closed model
// would open).
bool makeSymmetric(MeshEdit& e, int axis, int keepSide, std::string* why = nullptr);

} // namespace mazz
