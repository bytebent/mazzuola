#pragma once
// Where a view is on screen and how world points map to it: what tools need to pick,
// drag and draw their handles. Built per view and frame from its camera and image
// rectangle (logical pixels). Pure maths.
#include "core/camera.hpp"

namespace mazz {

struct ViewGeom {
    ViewDir dir = ViewDir::Top;
    OrthoCamera ortho; // 2D views
    Camera3D cam;      // 3D view
    double x0 = 0, y0 = 0, w = 1, h = 1; // image rectangle on screen

    bool is3D() const { return dir == ViewDir::Persp; }
    OrthoAxes axes() const { return orthoAxes(dir); } // 2D views only
    // Screen pixels per cm (2D views).
    double pxPerCm() const { return ortho.zoom; }

    // World point -> screen position. False if it can't be seen (behind the 3D camera).
    bool project(const Vec3& p, double& sx, double& sy) const;
    // Screen position -> a ray into the scene (unit direction). In 2D views the ray
    // starts far in front of the view and runs along the view direction.
    void ray(double sx, double sy, Vec3& origin, Vec3& dir) const;
    // 2D views: screen position -> plane coordinates (u right, v up) and back.
    void toPlane(double sx, double sy, double& u, double& v) const;
};

// Nearest hit of a ray with a triangle (Möller–Trumbore); t along the ray.
bool rayTriangle(const Vec3& o, const Vec3& d, const Vec3& a, const Vec3& b, const Vec3& c, double& t);

} // namespace mazz
