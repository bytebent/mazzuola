#pragma once
// Selection and editing operations on a mesh. Pure functions on core data; the editor
// wraps each change in an undoable command.
#include "core/mesh.hpp"
#include <set>
#include <vector>

namespace mazz {

enum class SelectMode { Vertex, Edge, Face, Piece };

// What is selected. Only the set matching the current mode is used; switching mode
// converts (see convertSelection). Piece mode stores faces.
struct Selection {
    std::set<int> verts;
    std::set<Edge> edges;
    std::set<int> faces;
    bool empty() const { return verts.empty() && edges.empty() && faces.empty(); }
    void clear() { verts.clear(); edges.clear(); faces.clear(); }
    bool operator==(const Selection& o) const { return verts == o.verts && edges == o.edges && faces == o.faces; }
};

// The points a selection moves (faces/edges -> their corners).
std::set<int> selectedVerts(const Mesh& m, const Selection& s, SelectMode mode);
// The faces a selection covers (vertices/edges -> faces whose corners are all selected).
std::set<int> facesFullySelected(const Mesh& m, const std::set<int>& verts);
// Converts a selection when the mode changes.
Selection convertSelection(const Mesh& m, const Selection& s, SelectMode from, SelectMode to);
// Drops anything that no longer exists or is hidden.
void cleanSelection(const Mesh& m, Selection& s);

// ---- topology
// Connected piece number for each face (faces sharing a point are connected).
std::vector<int> pieceIds(const Mesh& m);
std::set<int> pieceOf(const Mesh& m, int face);
// Edge loop through quads (stops at points not shared by exactly four edges).
std::set<Edge> edgeLoop(const Mesh& m, const Edge& start);
// Edge ring: across quads, from each edge to the opposite one.
std::set<Edge> edgeRing(const Mesh& m, const Edge& start);
void growSelection(const Mesh& m, Selection& s, SelectMode mode);
void shrinkSelection(const Mesh& m, Selection& s, SelectMode mode);
// Faces like the given ones: same texture, same smoothing group, or facing the same way.
enum class Similar { Texture, SmoothingGroup, Direction };
std::set<int> similarFaces(const Mesh& m, const std::set<int>& faces, Similar how, double maxAngleDeg = 1.0);

// ---- transforms
struct Affine {
    double m[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    Vec3 t;
    Vec3 apply(const Vec3& p) const;
    Affine then(const Affine& next) const; // next ∘ this
    double det() const;
};
Affine translation(const Vec3& d);
Affine rotationXYZ(const Vec3& degrees);          // about the origin, X then Y then Z
Affine scaling(const Vec3& factors);
Affine about(const Affine& a, const Vec3& pivot); // a, done around a pivot point
Affine mirrorAxis(int axis, const Vec3& pivot);

// Moves points. Faces whose points are all transformed get their winding flipped when
// the transform mirrors (negative determinant), so they keep facing outward.
void transformPoints(Mesh& m, const std::set<int>& verts, const Affine& a);
// Moves selected faces' points along the faces' average normal.
void moveAlongNormal(Mesh& m, const std::set<int>& faces, double distance);

struct CloneParams {
    int count = 1;
    Vec3 offset{0, 0, 0};        // per copy
    bool offsetIsTotal = false;  // offset is for all copies together
    Vec3 rotate{0, 0, 0};        // degrees per copy, about the pivot
    Vec3 scale{100, 100, 100};   // % per copy, about the pivot (multiplied each step)
    bool scaleOffset = false;    // each step's offset grows/shrinks with the scale
    Vec3 pivot{0, 0, 0};
};
// Copies faces; returns the new faces (the copies, all steps).
std::set<int> cloneFaces(Mesh& m, const std::set<int>& faces, const CloneParams& p);
std::set<int> duplicateFaces(Mesh& m, const std::set<int>& faces);
// Mirrors faces (or, with `copy`, a mirrored copy); returns the faces that ended up mirrored.
std::set<int> mirrorFaces(Mesh& m, const std::set<int>& faces, int axis, const Vec3& pivot, bool copy);
void flattenPoints(Mesh& m, const std::set<int>& verts, int axis, double value);
void snapPoints(Mesh& m, const std::set<int>& verts, double grid);

// Deletes what's selected (points/edges: the faces using them). Returns faces removed.
int deleteSelection(Mesh& m, const Selection& s, SelectMode mode);
void setHidden(Mesh& m, const std::set<int>& faces, bool hidden);
std::set<int> facesOfSelection(const Mesh& m, const Selection& s, SelectMode mode); // any face touched

// Joins pairs of neighbouring triangles into quads when they are (nearly) flat together
// and the quad is convex. Returns how many quads were made.
int joinTrianglesIntoQuads(Mesh& m, double maxAngleDeg, const std::set<int>* only = nullptr);

// Normal per face corner: faces in the same (non-zero) smoothing group share averaged
// normals at their common points; group 0 is flat.
std::vector<std::array<Vec3, 4>> cornerNormals(const Mesh& m);

} // namespace mazz
