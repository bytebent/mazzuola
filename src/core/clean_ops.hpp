#pragma once
// Clean-up commands (ARCHITECTURE §6, v0.2.5): merge, collapse, weld, dissolve, tear
// open, fill, detach, flip, flip diagonal, triangulate / join into quads, double-sided.
// Merging, collapsing and dissolving keep a closed model closed: they return false (the
// caller reverts) when the result would be open. Pure C++.
#include "core/document.hpp"
#include <set>
#include <string>

namespace mazz {

// Points become one at `at` (keeping point `keep`'s index if it is one of them, else
// the smallest). Faces that lose all area go; a face and its exact reverse both go.
bool mergePoints(MeshEdit& e, const std::set<int>& verts, const Vec3& at, int keep = -1);
// Each run of joined selected edges shrinks to one point at its middle.
bool collapseEdges(MeshEdit& e, const std::set<Edge>& edges);
// Points closer than `dist` (cm) become one (at the first of them); returns how many went.
int weldByDistance(MeshEdit& e, const std::set<int>& verts, double dist);

// Dissolve: the faces round the points / edges / faces join, the surface stays closed;
// joined faces over 4 corners are split again. False with `why` when it can't.
bool dissolvePoints(MeshEdit& e, const std::set<int>& verts, std::string* why = nullptr);
bool dissolveEdges(MeshEdit& e, const std::set<Edge>& edges, std::string* why = nullptr);
// Joins neighbouring selected faces into fewer; a face alone shrinks to a point.
bool dissolveFaces(MeshEdit& e, const std::set<int>& faces, std::set<int>* out = nullptr, std::string* why = nullptr);

// A face between 3 or 4 points (turned to match the faces round it).
bool fillFace(MeshEdit& e, const std::set<int>& verts, int* made = nullptr, std::string* why = nullptr);
// The faces get their own copies of the points they share with other faces.
bool detachFaces(MeshEdit& e, const std::set<int>& faces);
bool flipFaces(MeshEdit& e, const std::set<int>& faces);
bool flipDiagonals(MeshEdit& e, const std::set<int>& faces);
bool triangulate(MeshEdit& e, const std::set<int>& faces, std::set<int>* out = nullptr);
// Pairs of triangles sharing an edge that make a good flat, convex four-cornered face.
int joinIntoQuads(MeshEdit& e, const std::set<int>& faces, std::set<int>* out = nullptr);
bool setTwoSided(MeshEdit& e, const std::set<int>& faces, bool on);

} // namespace mazz
