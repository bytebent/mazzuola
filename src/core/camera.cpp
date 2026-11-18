#include "core/camera.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace mazz {

const char* viewDirName(ViewDir d) {
    switch (d) {
    case ViewDir::Top: return "Top";
    case ViewDir::Bottom: return "Bottom";
    case ViewDir::Front: return "Front";
    case ViewDir::Back: return "Back";
    case ViewDir::Left: return "Left";
    case ViewDir::Right: return "Right";
    case ViewDir::Persp: return "3D";
    }
    return "?";
}

bool viewDirFromName(const char* s, ViewDir& out) {
    for (int i = 0; i < kViewDirCount; ++i) {
        const char* n = viewDirName((ViewDir)i);
        size_t k = 0;
        while (s[k] && n[k] && std::tolower((unsigned char)s[k]) == std::tolower((unsigned char)n[k])) ++k;
        if (!s[k] && !n[k]) { out = (ViewDir)i; return true; }
    }
    return false;
}

// Z is up. Front looks along +Y (the model's front faces -Y), Right looks along -X.
OrthoAxes orthoAxes(ViewDir d) {
    switch (d) {
    case ViewDir::Top: return {0, 1, +1, +1};     // looks down
    case ViewDir::Bottom: return {0, 1, +1, -1};  // looks up
    case ViewDir::Front: return {0, 2, +1, +1};   // camera at -Y
    case ViewDir::Back: return {0, 2, -1, +1};    // camera at +Y
    case ViewDir::Right: return {1, 2, +1, +1};   // camera at +X
    case ViewDir::Left: return {1, 2, -1, +1};    // camera at -X
    case ViewDir::Persp: break;
    }
    return {0, 1, +1, +1};
}

// ---------------------------------------------------------------- 2D views

void OrthoCamera::screenToPlane(double px, double py, double w, double h, double& u, double& v) const {
    u = cu + (px - w / 2) / zoom;
    v = cv - (py - h / 2) / zoom;
}

void OrthoCamera::planeToScreen(double u, double v, double w, double h, double& px, double& py) const {
    px = w / 2 + (u - cu) * zoom;
    py = h / 2 - (v - cv) * zoom;
}

void OrthoCamera::worldToPlane(const Vec3& p, double& u, double& v) const {
    const OrthoAxes a = orthoAxes(dir);
    u = p[a.u] * a.su;
    v = p[a.v] * a.sv;
}

Vec3 OrthoCamera::planeToWorld(double u, double v, double depth) const {
    const OrthoAxes a = orthoAxes(dir);
    Vec3 p = a.right() * u + a.up() * v;
    p += a.forward() * -depth; // depth measured toward the viewer
    return p;
}

void OrthoCamera::pan(double dxPx, double dyPx) {
    cu -= dxPx / zoom;
    cv += dyPx / zoom;
}

void OrthoCamera::zoomAt(double factor, double px, double py, double w, double h) {
    double u0, v0;
    screenToPlane(px, py, w, h, u0, v0);
    zoom = std::clamp(zoom * factor, kMinZoom, kMaxZoom);
    double u1, v1;
    screenToPlane(px, py, w, h, u1, v1);
    cu += u0 - u1;
    cv += v0 - v1;
}

Mat4 OrthoCamera::view() const {
    const OrthoAxes a = orthoAxes(dir);
    // Eye far back along the view axis, centred on (cu, cv).
    const Vec3 centre = a.right() * cu + a.up() * cv;
    const Vec3 eye = centre - a.forward() * 50000.0;
    return lookAlong(eye, a.forward(), a.up());
}

Mat4 OrthoCamera::projection(double w, double h) const {
    const double hw = w / 2 / zoom, hh = h / 2 / zoom;
    return ortho(-hw, hw, -hh, hh, 1, 100000);
}

// ---------------------------------------------------------------- 3D view

Vec3 Camera3D::forward() const {
    // yaw = 0: looking along +Y (from the front, like the Front view); positive yaw
    // turns the camera to the left around Z. pitch > 0 looks down.
    const double y = radians(yaw), p = radians(pitch);
    return normalize({-std::sin(y) * std::cos(p), std::cos(y) * std::cos(p), -std::sin(p)});
}

Vec3 Camera3D::right() const { return normalize(cross(forward(), {0, 0, 1})); }
Vec3 Camera3D::up() const { return cross(right(), forward()); }

void Camera3D::orbit(double dYawDeg, double dPitchDeg) {
    yaw = std::fmod(yaw + dYawDeg, 360.0);
    pitch = std::clamp(pitch + dPitchDeg, -89.0, 89.0);
}

void Camera3D::panPx(double dxPx, double dyPx, double viewHeightPx) {
    // World size of one pixel at the target's distance.
    const double perPx = 2 * distance * std::tan(radians(fovy) / 2) / std::max(1.0, viewHeightPx);
    target -= right() * (dxPx * perPx);
    target += up() * (dyPx * perPx);
}

void Camera3D::dolly(double factor) { distance = std::clamp(distance * factor, kMinDistance, kMaxDistance); }

void Camera3D::look(double dYawDeg, double dPitchDeg) {
    const Vec3 e = eye();
    orbit(dYawDeg, dPitchDeg);
    target = e + forward() * distance; // eye stays put
}

void Camera3D::fly(double forwardAmt, double rightAmt, double upAmt) {
    target += forward() * forwardAmt + right() * rightAmt + Vec3{0, 0, upAmt};
}

Mat4 Camera3D::view() const { return lookAlong(eye(), forward(), {0, 0, 1}); }

Mat4 Camera3D::projection(double aspect) const {
    const double n = std::max(0.1, distance / 1000);
    return perspective(fovy, aspect, n, std::max(100000.0, distance * 100));
}

} // namespace mazz
