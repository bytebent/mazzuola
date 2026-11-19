#include "core/view_geom.hpp"
#include <cmath>

namespace mazz {

bool ViewGeom::project(const Vec3& p, double& sx, double& sy) const {
    if (!is3D()) {
        double u, v;
        ortho.worldToPlane(p, u, v);
        ortho.planeToScreen(u, v, w, h, sx, sy);
        sx += x0;
        sy += y0;
        return true;
    }
    // In front of the camera?
    const Vec3 rel = p - cam.eye();
    const double depth = dot(rel, cam.forward());
    if (depth <= 1e-6) return false;
    const double t = std::tan(radians(cam.fovy) / 2);
    const double aspect = w / h;
    const double nx = dot(rel, cam.right()) / (depth * t * aspect);
    const double ny = dot(rel, cam.up()) / (depth * t);
    sx = x0 + (nx + 1) / 2 * w;
    sy = y0 + (1 - ny) / 2 * h;
    return true;
}

void ViewGeom::ray(double sx, double sy, Vec3& origin, Vec3& d) const {
    if (!is3D()) {
        double u, v;
        toPlane(sx, sy, u, v);
        const OrthoAxes ax = axes();
        d = normalize(ax.forward()); // the direction the view looks along
        origin = ortho.planeToWorld(u, v) - d * 1e6;
        return;
    }
    const double t = std::tan(radians(cam.fovy) / 2);
    const double aspect = w / h;
    const double nx = (sx - x0) / w * 2 - 1, ny = 1 - (sy - y0) / h * 2;
    origin = cam.eye();
    d = normalize(cam.forward() + cam.right() * (nx * t * aspect) + cam.up() * (ny * t));
}

void ViewGeom::toPlane(double sx, double sy, double& u, double& v) const {
    ortho.screenToPlane(sx - x0, sy - y0, w, h, u, v);
}

bool rayTriangle(const Vec3& o, const Vec3& d, const Vec3& a, const Vec3& b, const Vec3& c, double& t) {
    const Vec3 e1 = b - a, e2 = c - a;
    const Vec3 p = cross(d, e2);
    const double det = dot(e1, p);
    if (std::fabs(det) < 1e-12) return false;
    const double inv = 1 / det;
    const Vec3 s = o - a;
    const double u = dot(s, p) * inv;
    if (u < 0 || u > 1) return false;
    const Vec3 q = cross(s, e1);
    const double v = dot(d, q) * inv;
    if (v < 0 || u + v > 1) return false;
    t = dot(e2, q) * inv;
    return t > 0;
}

} // namespace mazz
