#include "core/ref_image.hpp"
#include "core/numbers.hpp"
#include <algorithm>
#include <cmath>

namespace mazz {

void RefPlacement::corners(double u[4], double v[4]) const {
    double c, s;
    cosSinDeg(rot, c, s);
    const double hw = width / 2, hh = height() / 2;
    const double lu[4] = {-hw, hw, hw, -hw}, lv[4] = {-hh, -hh, hh, hh};
    for (int k = 0; k < 4; ++k) {
        u[k] = cu + lu[k] * c - lv[k] * s;
        v[k] = cv + lu[k] * s + lv[k] * c;
    }
}

bool RefPlacement::toPicture(double u, double v, double& s, double& t) const {
    double c, sn;
    cosSinDeg(rot, c, sn);
    const double du = u - cu, dv = v - cv;
    const double lu = du * c + dv * sn, lv = -du * sn + dv * c; // un-rotate
    s = lu / width + 0.5;
    t = lv / height() + 0.5;
    return s >= 0 && s <= 1 && t >= 0 && t <= 1;
}

void RefPlacement::fit(double u0, double v0, double u1, double v1, double a) {
    aspect = a > 0 ? a : 1;
    const double w = std::fabs(u1 - u0), h = std::fabs(v1 - v0);
    width = std::min(w, h / aspect);
    cu = (u0 + u1) / 2;
    cv = (v0 + v1) / 2;
    rot = 0;
}

void RefPlacement::dragCorner(int fixed, double u, double v, double minWidth, double step) {
    double U[4], V[4];
    corners(U, V);
    const int moving = (fixed + 2) % 4;
    // Along the diagonal from the fixed corner: the new size is the projection of the
    // pointer onto it (proportions kept), never flipping over.
    const double du = U[moving] - U[fixed], dv = V[moving] - V[fixed];
    const double len2 = du * du + dv * dv;
    if (len2 <= 0) return;
    const double k = ((u - U[fixed]) * du + (v - V[fixed]) * dv) / len2;
    double newWidth = std::max(minWidth, width * k);
    if (step > 0) newWidth = std::max(step, std::round(newWidth / step) * step);
    const double f = newWidth / width;
    cu = U[fixed] + du * f / 2;
    cv = V[fixed] + dv * f / 2;
    width = newWidth;
}

} // namespace mazz
