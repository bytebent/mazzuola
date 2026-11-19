#pragma once
// Picking whole pieces in a view (ARCHITECTURE §6 Selection).
#include "core/mesh.hpp"
#include "core/view_geom.hpp"
#include <set>
#include <vector>

namespace mazz {

// Pieces under a screen point, nearest to the viewer first. In 2D views a piece counts
// when the point is inside one of its faces as seen from the view, or within `tolPx`
// of one of its edges; in the 3D view, the face the ray hits first decides (one piece).
std::vector<int> piecesAt(const Mesh& m, const ViewGeom& g, double sx, double sy, double tolPx = 4);

// Pieces lying entirely inside a screen rectangle (any corner order).
std::set<int> piecesInRect(const Mesh& m, const ViewGeom& g, double x0, double y0, double x1, double y1);

// All faces of the given pieces (piece numbers from pieceIds).
std::set<int> facesOfPieces(const Mesh& m, const std::set<int>& pieces);

} // namespace mazz
