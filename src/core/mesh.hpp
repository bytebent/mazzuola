#pragma once
// The model's geometry (ARCHITECTURE §5): points and faces (triangles or quads) that
// refer to them by index. Faces sharing points are connected; a connected set of faces
// is a "piece". Positions are always clean (core/numbers.hpp).
#include "core/math.hpp"
#include <cstdint>
#include <vector>

namespace mazz {

struct Vec2 {
    double x = 0, y = 0;
    bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
};

struct Rgb {
    std::uint8_t r = 255, g = 255, b = 255;
    bool operator==(const Rgb& o) const { return r == o.r && g == o.g && b == o.b; }
};

struct Face {
    int n = 4;                 // 3 or 4 corners
    int v[4] = {0, 0, 0, 0};   // point indices, counter-clockwise seen from outside
    Vec2 uv[4];                // texture coordinate per corner (milestone 5)
    Rgb col[4];                // vertex colour per corner (milestone 7)
    int tex = -1;              // texture index, -1 = none
    int group = -1;            // group index, -1 = none (milestone 3)
    int sg = 0;                // smoothing group, 0 = flat
    bool twoSided = false;
    bool diag = false;         // quad split along v1-v3 instead of v0-v2

    bool operator==(const Face& o) const;
    bool operator!=(const Face& o) const { return !(*this == o); }
};

struct Mesh {
    std::vector<Vec3> verts;
    std::vector<Face> faces;

    bool empty() const { return faces.empty(); }
    // Bounds of all points (false if there are none).
    bool bounds(Vec3& mn, Vec3& mx) const;
};

// Normal of a face (unit length; zero for a degenerate face).
Vec3 faceNormal(const Mesh& m, int face);
Vec3 faceCentre(const Mesh& m, int face);

// Piece number of every face (faces sharing a point are in the same piece), numbered
// 0, 1, 2... in order of their lowest face. Returns the number of pieces.
int pieceIds(const Mesh& m, std::vector<int>& pieceOfFace);

// A closed box between two corners, six outward-facing quads (corners cleaned).
void addBox(Mesh& m, const Vec3& a, const Vec3& b);

} // namespace mazz
