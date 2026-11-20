#include "core/figure.hpp"
#include "core/numbers.hpp"
#include <algorithm>
#include <cmath>

namespace mazz {

namespace {
// Laid out on a 72-tall body and scaled up, as in Scalpellino.
constexpr double kScale = kFigureHeight / 72.0;
constexpr double kHalfDepth = 5 * kScale; // its extent along X from the feet's centre
constexpr double kGap = 25;               // cm between the model's box and the figure
} // namespace

std::vector<std::pair<Vec3, Vec3>> figureBoxes(const Vec3& at) {
    auto box = [&](double x0, double y0, double z0, double x1, double y1, double z1) {
        return std::make_pair(at + Vec3{x0, y0, z0} * kScale, at + Vec3{x1, y1, z1} * kScale);
    };
    return {
        box(-4, -9, 0, 4, -1, 34),    // legs
        box(-4, 1, 0, 4, 9, 34),
        box(-5, -10, 34, 5, 10, 60),  // body
        box(-3, -15, 35, 3, -10, 60), // arms
        box(-3, 10, 35, 3, 15, 60),
        box(-5, -5, 62, 5, 5, 72),    // head
    };
}

Vec3 figureSpot(const Mesh& m) {
    double minX = 0;
    Vec3 mn, mx;
    if (m.bounds(mn, mx)) minX = std::min(minX, mn.x);
    // Whole centimetres, so the spot reads cleanly.
    return {std::floor(minX - kGap - kHalfDepth), 0, 0};
}

} // namespace mazz
