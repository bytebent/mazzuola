#pragma once
// What is under the cursor in a view, and box selection. Pure maths: works from the
// view's matrix and size, so it's testable without graphics.
#include "core/mesh.hpp"
#include "core/ops.hpp"

namespace mazz {

// Maps world points to a view's pixels (x right, y down, origin top-left; logical px).
struct Projector {
    Mat4 viewProj;
    double w = 1, h = 1;
    bool perspective = false;
    // Screen position and depth (larger = farther). False if behind the camera.
    bool project(const Vec3& p, double& x, double& y, double& depth) const;
    // The ray through a pixel: origin and unit direction.
    void ray(double x, double y, Vec3& origin, Vec3& dir) const;
};

struct FaceHit {
    int face = -1;
    double t = 0; // distance along the ray
};
// Nearest visible face hit by the ray through (x, y).
FaceHit pickFace(const Mesh& m, const Projector& pr, double x, double y);
// Nearest point / edge within `radius` pixels. In a perspective (3D) view, points and
// edges hidden behind faces are skipped; 2D views see through (like Hammer).
int pickVertex(const Mesh& m, const Projector& pr, double x, double y, double radius);
bool pickEdge(const Mesh& m, const Projector& pr, double x, double y, double radius, Edge& out);
// Everything inside a screen rectangle: points inside; edges and faces with all their
// points inside. Hidden faces (and their points/edges) are ignored.
Selection boxSelect(const Mesh& m, const Projector& pr, double x0, double y0, double x1, double y1, SelectMode mode);

} // namespace mazz
