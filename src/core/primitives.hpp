#pragma once
// Primitive shapes (ARCHITECTURE §6 Primitives). Each one fills its box exactly (what
// you see in the box is what you get) and stands along `axis` (the depth axis of the
// view it was drawn in: drawn in Top = standing up / lying flat on the ground). Faces
// point outward, round sides share a smoothing group, flat caps stay flat, positions
// are clean. Pure C++.
#include "core/mesh.hpp"
#include "core/rock.hpp"
#include <string>

namespace mazz {

// The rock (a generator) is drawn the same way, so it is one of the kinds too.
enum class PrimKind { Box, Plane, Cylinder, Cone, Tube, Torus, Sphere, Geosphere, Rock };
constexpr int kPrimKindCount = 9;
const char* primKindName(PrimKind k); // "Box", "Plane", ...
bool primKindFromName(const std::string& s, PrimKind& out); // case-insensitive

struct PrimParams {
    PrimKind kind = PrimKind::Box;
    // box
    int segX = 1, segY = 1, segZ = 1;
    // plane
    int rows = 1, cols = 1;
    bool flipDiagonals = false;
    // cylinder, cone, tube, torus, sphere
    int sides = 8;
    int segments = 1;     // cylinder/cone/tube: along the axis
    int capRings = 1;     // cylinder/cone: rings on the flat caps
    double inner = 50;    // tube: hole size, % of the outer size
    int tubeSides = 6;    // torus: around the tube
    double tubeWidth = 50; // torus: tube width across the ring, % of the radius
    int stacks = 6;       // sphere: from pole to pole
    int iterations = 1;   // geosphere: subdivisions of an icosahedron
    RockParams rock;      // rock

    bool operator==(const PrimParams& o) const {
        return kind == o.kind && segX == o.segX && segY == o.segY && segZ == o.segZ && rows == o.rows &&
               cols == o.cols && flipDiagonals == o.flipDiagonals && sides == o.sides && segments == o.segments &&
               capRings == o.capRings && inner == o.inner && tubeSides == o.tubeSides && tubeWidth == o.tubeWidth &&
               stacks == o.stacks && iterations == o.iterations && rock.faces == o.rock.faces &&
               rock.roughness == o.rock.roughness && rock.flatBottom == o.rock.flatBottom &&
               rock.bottomHeight == o.rock.bottomHeight && rock.joinQuads == o.rock.joinQuads && rock.seed == o.rock.seed;
    }
};

// Settings brought into their allowed ranges.
PrimParams clampParams(const PrimParams& p);

// The shape filling the box mn..mx, standing along `axis` (0/1/2).
Mesh makePrimitive(const PrimParams& p, const Vec3& mn, const Vec3& mx, int axis);

} // namespace mazz
