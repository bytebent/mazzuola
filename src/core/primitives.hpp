#pragma once
// Primitives: plane, box, cylinder, cone, tube, torus, sphere, geosphere, and the rock
// generator. Each fills a box (size, centred on `centre`); round shapes stand along
// `axis` (0 = X, 1 = Y, 2 = Z). Results are closed (except the plane), wound outward,
// and pass checkMesh.
#include "core/mesh.hpp"
#include <cstdint>
#include <string>

namespace mazz {

enum class PrimKind { Plane, Box, Cylinder, Cone, Tube, Torus, Sphere, Geosphere, Rock };
constexpr int kPrimKindCount = 9;
const char* primKindName(PrimKind k);       // "Plane", "Box", ...
bool primKindFromName(const std::string& s, PrimKind& out);

struct PrimParams {
    PrimKind kind = PrimKind::Box;
    Vec3 centre{0, 0, 0};
    Vec3 size{64, 64, 64};
    int axis = 2;

    // plane
    int rows = 1, cols = 1;
    bool flipDiagonals = false;
    // box
    int segX = 1, segY = 1, segZ = 1;
    double round = 0;      // rounded edges radius (cm), 0 = sharp
    int roundSegs = 2;
    // cylinder / cone / tube / torus / sphere
    int sides = 8;
    int stacks = 1;        // segments along the axis (sphere: from pole to pole, >= 2)
    int capRings = 1;      // rings on flat caps
    double inner = 0.5;    // tube: inner / outer radius
    int rings = 6;         // torus: segments around the tube
    // geosphere
    int iterations = 1;
    // rock
    int faces = 24;        // target triangle count (12..60)
    double roughness = 35; // %
    Vec3 stretch{100, 80, 60}; // % (the box size still decides the final size)
    bool flatBottom = true;
    double bottomHeight = 15;  // % of the height cut off
    bool joinQuads = false;
    uint32_t seed = 1;
    bool smooth = true;    // rock: one smoothing group, else flat
};

// Valid ranges are enforced here (counts clamped, sizes at least 0.01 cm).
PrimParams clampParams(const PrimParams& p);
Mesh makePrimitive(const PrimParams& p);

} // namespace mazz
