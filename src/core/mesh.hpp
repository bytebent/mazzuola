#pragma once
// The model's geometry: points (vertices) and faces (triangles or quads).
// Pure C++; no graphics. Units: cm, Z up.
#include "core/math.hpp"
#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace mazz {

struct Vec2 {
    double x = 0, y = 0;
};

struct Rgb {
    uint8_t r = 255, g = 255, b = 255;
    bool operator==(const Rgb& o) const { return r == o.r && g == o.g && b == o.b; }
};

struct Face {
    int n = 3;                          // 3 or 4 corners
    std::array<int, 4> v{{0, 0, 0, 0}}; // vertex indices, counter-clockwise seen from outside
    std::array<Vec2, 4> uv{};           // texture coordinates in texture pixels (milestone 5)
    std::array<Rgb, 4> col{};           // vertex colours per corner (milestone 7)
    int tex = -1;                       // texture index, -1 = none
    int part = -1;                      // part index, -1 = none (milestone 3)
    int sg = 0;                         // smoothing group, 0 = flat
    bool twoSided = false;
    bool diag = false;                  // quads: false = split 0-2, true = split 1-3
    bool hidden = false;                // editor-only visibility

    // The face's triangles as corner indices (into v), 1 or 2 of them.
    int triangles(std::array<std::array<int, 3>, 2>& out) const;
};

using Edge = std::pair<int, int>; // vertex indices, first < second
inline Edge makeEdge(int a, int b) { return a < b ? Edge{a, b} : Edge{b, a}; }

struct Mesh {
    std::vector<Vec3> verts;
    std::vector<Face> faces;

    int addVertex(const Vec3& p) { verts.push_back(p); return (int)verts.size() - 1; }
    int addTri(int a, int b, int c, int sg = 0);
    int addQuad(int a, int b, int c, int d, int sg = 0);
    // Appends another mesh (its vertex indices shifted). Returns the index of its first face.
    int append(const Mesh& other);

    int triangleCount(bool includeHidden = true) const;
    Vec3 faceNormal(int f) const;   // unit; average over the face's triangles
    Vec3 faceCentre(int f) const;
    double faceArea(int f) const;
    bool bounds(Vec3& mn, Vec3& mx) const; // false if empty
    std::vector<Edge> edges(bool includeHidden = true) const; // unique, sorted

    // Removes vertices no face uses, renumbering the faces. Returns how many were removed.
    int removeUnusedVertices();
    // Removes faces whose flag is set; then unused vertices.
    void removeFaces(const std::vector<bool>& remove);
};

// Problems that should never exist after any tool (used by tests; the problems list in
// milestone 3 builds on these).
struct MeshCheck {
    int badIndices = 0;      // a corner refers to a missing vertex
    int repeatedCorners = 0; // a face uses the same vertex twice
    int zeroArea = 0;        // a face with (almost) no area
    int badCornerCount = 0;  // not 3 or 4
};
MeshCheck checkMesh(const Mesh& m, double minArea = 1e-9);
bool meshOk(const Mesh& m);

// Closed surface: every edge is used by exactly two faces, in opposite directions
// (consistent winding).
bool isClosed(const Mesh& m);
// Signed volume (positive when faces point outward).
double signedVolume(const Mesh& m);

} // namespace mazz
