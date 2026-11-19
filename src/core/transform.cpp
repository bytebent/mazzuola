#include "core/transform.hpp"
#include "core/numbers.hpp"
#include <cmath>
#include <map>

namespace mazz {

Vec3 stretchInBox(const Vec3& p, const Vec3& fromMn, const Vec3& fromMx, const Vec3& toMn, const Vec3& toMx) {
    Vec3 out;
    for (int a = 0; a < 3; ++a) {
        const double size = fromMx[a] - fromMn[a];
        if (size <= 0) out[a] = p[a] + (toMn[a] - fromMn[a]);
        else out[a] = toMn[a] + (p[a] - fromMn[a]) * ((toMx[a] - toMn[a]) / size);
    }
    return out;
}

Vec3 rotateAbout(const Vec3& p, const Vec3& centre, const Vec3& axis, double degrees) {
    double c = 1, s = 0;
    cosSinDeg(degrees, c, s);
    const Vec3 k = normalize(axis);
    const Vec3 v = p - centre;
    // Rodrigues: v cos + (k x v) sin + k (k.v)(1 - cos)
    return centre + v * c + cross(k, v) * s + k * (dot(k, v) * (1 - c));
}

double snapAngle(double degrees, double step) {
    if (step <= 0) return degrees;
    return std::round(degrees / step) * step;
}

std::set<int> copyFaces(MeshEdit& e, const std::set<int>& faces) {
    std::map<int, int> newPoint;
    std::set<int> made;
    const Mesh& m = e.mesh();
    // Collect first: adding to the mesh may move its storage.
    std::vector<Face> src;
    for (int f : faces)
        if (f >= 0 && f < (int)m.faces.size()) src.push_back(m.faces[f]);
    for (Face f : src) {
        for (int k = 0; k < f.n; ++k) {
            auto it = newPoint.find(f.v[k]);
            if (it == newPoint.end()) {
                const Vec3 p = e.mesh().verts[f.v[k]];
                it = newPoint.emplace(f.v[k], e.addVert(p)).first;
            }
            f.v[k] = it->second;
        }
        made.insert(e.addFace(f));
    }
    return made;
}

} // namespace mazz
