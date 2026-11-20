#pragma once
// Drawing on faces (ARCHITECTURE §6 Draw, v0.2.2): rectangles, circles and lines drawn
// onto one face cut it into new faces of 3 or 4 corners. Faces next to it that share a
// cut edge get the new points too, so a closed surface stays closed. Pure C++.
#include "core/document.hpp"
#include <set>
#include <string>
#include <vector>

namespace mazz {

// A face's own flat frame: origin at its first point, u along its first edge, v across
// (n × u), n its normal. False for a face without area.
bool faceFrame(const Mesh& m, int face, Vec3& origin, Vec3& u, Vec3& v, Vec3& n);

// Splits a flat polygon (point indices, counter-clockwise seen from `normal`; points
// in a straight line allowed) into faces of 3 or 4 corners, as few as it can (convex
// four-cornered pieces). Empty if it can't (a crossed outline).
std::vector<std::vector<int>> splitPolygon(const Mesh& m, const std::vector<int>& poly, const Vec3& normal);

// Where a point lies relative to a face, in its plane.
enum class FacePlace { Outside, Inside, OnEdge, OnCorner };
FacePlace placeOnFace(const Mesh& m, int face, const Vec3& p, int* edgeIndex = nullptr, int* corner = nullptr);

// Cuts a shape into `face`. `pts` lie in the face's plane:
//  - closed (rectangle, circle; counter-clockwise or not): inside the face, or touching
//    its edge along one stretch (a door from the floor);
//  - open (line): from an edge (or corner) of the face to another, through its inside.
// `shape` gets the faces of the closed shape (open: the smaller of the two parts).
// False with `why` set when the shape doesn't fit.
bool cutShape(MeshEdit& e, int face, const std::vector<Vec3>& pts, bool closed, std::set<int>* shape,
              std::string* why = nullptr);

// Points of a rectangle (corner, width along u, height along v) and of a circle (centre,
// radius, segments) in a face's frame, counter-clockwise seen from n.
std::vector<Vec3> rectanglePoints(const Vec3& corner, const Vec3& u, const Vec3& v, double w, double h);
std::vector<Vec3> circlePoints(const Vec3& centre, const Vec3& u, const Vec3& v, double radius, int segments);

} // namespace mazz
