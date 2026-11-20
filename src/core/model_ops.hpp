#pragma once
// Modelling operations of milestone 2, v0.2.0 (ARCHITECTURE §6 Push/pull, Extrude,
// Inset). Each works through a MeshEdit (undoable, results clean) and keeps faces facing
// outward: a closed surface stays closed. They return false (and change nothing that
// matters: the caller reverts) when the result would be invalid, e.g. an inset too wide
// for its face or a push that turns faces inside out.
#include "core/document.hpp"
#include <set>
#include <vector>

namespace mazz {

// ---- push/pull: one face along its normal
enum class PushKind {
    Stretch,  // the faces around it lie at right angles to it: they stretch
    NewFaces, // new side faces join it to where it was
    Solid,    // a face with an open side (a flat shape): becomes a solid (original kept, turned round)
};
// What pushing `face` does (forceNew: Cmd held when the drag started).
PushKind pushKind(const Mesh& m, int face, bool forceNew);
// Pushes `face` by `distance` cm along its normal (negative: in). On success `moved`
// (if given) is the pushed face's index afterwards.
bool pushPull(MeshEdit& e, int face, double distance, bool forceNew, int* moved = nullptr);
// The same for several faces lying flat together (a drawn shape split into pieces):
// they go as one. `moved` gets where they are afterwards.
PushKind pushKind(const Mesh& m, const std::set<int>& faces, bool forceNew);
bool pushPullFaces(MeshEdit& e, const std::set<int>& faces, double distance, bool forceNew, std::set<int>* moved = nullptr);

// ---- push/pull in edge and point mode: an along-normal dragger (no new faces)
// The surface's normal at an edge (faces using both its points) or at points (faces
// using any of them): the average of those faces' normals, unit length; zero if none
// or they cancel out.
Vec3 surfaceNormal(const Mesh& m, const std::set<int>& verts, bool edge);
// Moves the points by distance along dir; the faces around tilt. False if one would
// vanish or turn over.
bool pushPoints(MeshEdit& e, const std::set<int>& verts, const Vec3& dir, double distance);

// ---- extrude
struct ExtrudeParams {
    bool each = false;         // each face on its own (else the selection as one region)
    bool straight = false;     // straight along `axis` (else along the faces' normals)
    int axis = 2;              // 0 X, 1 Y, 2 Z
    double distance = 16;      // cm
    bool keepOriginal = false; // leaves a turned-round copy behind (a flat shape becomes a solid)
};
// Extrudes faces; `out` gets the new end faces. False if nothing to do or invalid.
bool extrudeFaces(MeshEdit& e, const std::set<int>& faces, const ExtrudeParams& p, std::set<int>* out = nullptr);
// Open edges (used by one face) of a set of edges.
std::set<Edge> openEdges(const Mesh& m, const std::set<Edge>& edges);
// Extrudes open edges straight by `offset`, a strip of new faces continuing the surface;
// `out` gets the new far edges. Edges that aren't open are left alone.
bool extrudeEdges(MeshEdit& e, const std::set<Edge>& edges, const Vec3& offset, std::set<Edge>* out = nullptr);

// ---- inset
struct InsetParams {
    bool each = false;   // each face on its own (else the selection as one region)
    double width = 4;    // cm, in the faces' planes
    double depth = 0;    // cm the inner faces are pushed in, with straight walls (negative: out)
};
// Insets faces; `out` gets the inner faces. False if too wide (the inner faces would
// turn over or vanish) or nothing to do.
bool inset(MeshEdit& e, const std::set<int>& faces, const InsetParams& p, std::set<int>* out = nullptr);

// ---- checks used by the operations and the tests
// Every edge used once in each direction (a closed, consistently turned surface).
bool isClosed(const Mesh& m);
// Signed volume (positive when a closed surface faces outward).
double signedVolume(const Mesh& m);

} // namespace mazz
