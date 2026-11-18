#pragma once
// View cameras: the orthographic directions of the 2D views and the 3D camera
// (orbit around a point, or fly). Pure maths; the render layer only reads the matrices.
#include "core/math.hpp"

namespace mazz {

// What a view looks at. The six orthographic directions plus the 3D view.
enum class ViewDir { Top, Bottom, Front, Back, Left, Right, Persp };
constexpr int kViewDirCount = 7;

const char* viewDirName(ViewDir d);           // "Top", "Front", ... "3D"
bool viewDirFromName(const char* s, ViewDir& out); // case-insensitive, accepts "3d"

// For an orthographic direction: world axis and sign shown to the right (u), up (v),
// and the direction the camera looks along (forward = -toward viewer).
struct OrthoAxes {
    int u, v;        // world axis indices
    double su, sv;   // +1 or -1
    Vec3 right() const { return axisVec(u) * su; }
    Vec3 up() const { return axisVec(v) * sv; }
    Vec3 forward() const { return cross(up(), right()); } // right-handed: right x up = toward viewer
};
OrthoAxes orthoAxes(ViewDir d); // d must not be Persp

// A 2D view: centre (in its own u/v plane coordinates) and zoom (logical px per cm).
struct OrthoCamera {
    ViewDir dir = ViewDir::Top;
    double cu = 0, cv = 0;
    double zoom = 2.0;

    static constexpr double kMinZoom = 0.01, kMaxZoom = 2000;

    // Logical pixel (x right, y down, origin top-left) in a w x h view -> plane coords.
    void screenToPlane(double px, double py, double w, double h, double& u, double& v) const;
    void planeToScreen(double u, double v, double w, double h, double& px, double& py) const;
    // World point -> plane coords.
    void worldToPlane(const Vec3& p, double& u, double& v) const;
    // Plane coords (+ depth along the view axis, default 0) -> world point.
    Vec3 planeToWorld(double u, double v, double depth = 0) const;

    void pan(double dxPx, double dyPx);                  // drag by logical pixels
    void zoomAt(double factor, double px, double py, double w, double h); // keeps the point under the cursor

    Mat4 view() const;
    Mat4 projection(double w, double h) const;
};

// The 3D view: orbits around `target`, or flies freely.
struct Camera3D {
    Vec3 target{0, 0, 0};
    double yaw = 35;       // degrees around Z, 0 = looking along +X... see eye()
    double pitch = 25;     // degrees above the ground plane (looking down when > 0)
    double distance = 400; // cm from target
    double fovy = 60;

    static constexpr double kMinDistance = 1, kMaxDistance = 100000;

    // Direction the camera looks along (unit).
    Vec3 forward() const;
    Vec3 right() const;
    Vec3 up() const;
    Vec3 eye() const { return target - forward() * distance; }

    void orbit(double dYawDeg, double dPitchDeg);
    void panPx(double dxPx, double dyPx, double viewHeightPx); // moves the target in the view plane
    void dolly(double factor);                                 // distance *= factor (clamped)

    // Fly: the eye moves, the target follows at the same distance.
    void look(double dYawDeg, double dPitchDeg);
    void fly(double forwardAmt, double rightAmt, double upAmt);

    Mat4 view() const;
    Mat4 projection(double aspect) const;
};

} // namespace mazz
