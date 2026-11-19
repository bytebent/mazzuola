#pragma once
// Selecting points, edges and faces (ARCHITECTURE §6 Selection, "Points, edges, faces").
#include "core/mesh.hpp"
#include "core/selection.hpp"
#include "core/view_geom.hpp"
#include <set>
#include <vector>

namespace mazz {

// Switching mode converts the selection: pieces → faces keeps their faces; faces →
// points/edges takes their corners/sides; points/edges → faces (and pieces) takes the
// faces whose corners/sides are all selected; points ↔ edges likewise.
Selection convertSelection(const Mesh& m, const Selection& s, SelectMode to);

// Picking at a screen point.
//  points: 2D = every point stacked at the nearest one's screen spot; 3D = the nearest
//          visible point. Empty when nothing is within tolPx.
//  edges:  the edges within tolPx, nearest to the viewer first (3D: visible ones).
//  faces:  the faces under the point, nearest to the viewer first.
std::set<int> pointsAt(const Mesh& m, const ViewGeom& g, double sx, double sy, double tolPx = 6);
std::vector<Edge> edgesAt(const Mesh& m, const ViewGeom& g, double sx, double sy, double tolPx = 5);
std::vector<int> facesAt(const Mesh& m, const ViewGeom& g, double sx, double sy);

// What a rectangle selects in `mode`: in 2D views everything inside (they see through),
// in the 3D view only what can be seen. Edges: both ends inside; faces: all corners.
Selection inRect(const Mesh& m, const ViewGeom& g, double x0, double y0, double x1, double y1, SelectMode mode);

// Whether a point can be seen from the 3D camera (no face in front of it).
bool pointVisible(const Mesh& m, const ViewGeom& g, int point);

// Grow / shrink by one step of neighbours (pieces: unchanged).
Selection growSelection(const Mesh& m, const Selection& s);
Selection shrinkSelection(const Mesh& m, const Selection& s);

// The edge loop through `e` (continuing straight through four-sided corners, all the
// way round or to where it can't go on), and the edge ring (the opposite sides across
// a strip of quads).
std::set<Edge> edgeLoop(const Mesh& m, Edge e);
std::set<Edge> edgeRing(const Mesh& m, Edge e);

// The faces Delete removes for a selection: the selected faces, or the faces that use
// a selected point or edge.
std::set<int> facesToDelete(const Mesh& m, const Selection& s);

} // namespace mazz
