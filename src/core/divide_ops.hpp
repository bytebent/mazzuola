#pragma once
// Cutting and dividing (ARCHITECTURE §6 Cut / Slide / Divide, v0.2.4): knife, loop
// cut, subdivide, divide edges, poke, connect, and the edge a point slides along.
// Faces next to a cut get the new points, so a closed model stays closed. Pure C++.
#include "core/document.hpp"
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mazz {

// Knife: points on edges or corners; each two in a row must lie on one face, which is
// cut between them. False with `why` when a step can't be cut (the caller reverts).
bool knife(MeshEdit& e, const std::vector<Vec3>& pts, std::string* why = nullptr);

// A loop cut's ring: the quads in line with an edge, going round through the edges
// opposite each other. `rails` are the ring's edges as (side 1, side 2) points, in
// order; `faces[i]` lies between rails[i] and rails[i + 1] (closed: the last face
// joins the last rail to the first).
struct LoopRing {
    std::vector<std::pair<int, int>> rails;
    std::vector<int> faces;
    bool closed = false;
    bool valid() const { return !faces.empty(); }
};
LoopRing loopRing(const Mesh& m, const Edge& e);
// Where the cuts would go (for the preview): one segment per cut per face.
std::vector<std::pair<Vec3, Vec3>> loopCutLines(const Mesh& m, const LoopRing& r, int cuts, double position);
// Cuts it: `cuts` lines through every quad of the ring; one cut sits at `position`
// (0..1 across the ring's edges), more are spread evenly. `newEdges` gets the cut edges.
bool loopCut(MeshEdit& e, const LoopRing& r, int cuts, double position, std::set<Edge>* newEdges = nullptr);

// Subdivide faces: quads into n x m (along their first edge, then across), triangles
// into n x n. Shared edges must split the same way (else false, with `why`).
bool subdivide(MeshEdit& e, const std::set<int>& faces, int n, int m, std::set<int>* out = nullptr,
               std::string* why = nullptr);
// Divide edges into n pieces.
bool divideEdges(MeshEdit& e, const std::set<Edge>& edges, int n, std::string* why = nullptr);
// Poke: a point in the middle of each face, `height` cm out along its normal; the face
// becomes a fan of triangles.
bool poke(MeshEdit& e, const std::set<int>& faces, double height, std::set<int>* out = nullptr);
// Connect two points of one face with a new edge (the face splits in two).
bool connectPoints(MeshEdit& e, int a, int b, std::string* why = nullptr);

// Slide: of point v's edges, the other end of the one closest to direction `dir`
// (-1: none).
int slideTarget(const Mesh& m, int v, const Vec3& dir);

} // namespace mazz
