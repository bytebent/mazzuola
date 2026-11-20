#pragma once
// The scale figure (ARCHITECTURE §5): a blocky person 175 cm tall, the same boxes as
// Scalpellino's, facing +X. Shown for scale only: never part of the model, never
// exported or saved. Pure C++.
#include "core/mesh.hpp"
#include <utility>
#include <vector>

namespace mazz {

constexpr double kFigureHeight = 175; // cm

// The figure's boxes (min, max corners) with its feet at `at`.
std::vector<std::pair<Vec3, Vec3>> figureBoxes(const Vec3& at);

// Where it stands: on the ground (z = 0) beside the origin on the -X side, just
// outside the model's box (and never closer to the origin than an empty model allows).
Vec3 figureSpot(const Mesh& m);

} // namespace mazz
