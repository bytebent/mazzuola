#pragma once
// Editor grid: size, visibility, snapping. Pure logic, no graphics.
// Units are centimetres. Sizes are powers of two from 1/8 cm to 512 cm, the same steps
// as Scalpellino plus the small ones Mazzuola needs for small objects (parked in
// docs/PLAN.md: match Scalpellino's cm steps once they are settled there).
#include <algorithm>
#include <cmath>

namespace mazz {

struct Grid {
    static constexpr double kMin = 0.125;
    static constexpr double kMax = 512;

    double size = 8;
    bool visible = true;
    bool snap = true;

    void halve()    { if (size / 2 >= kMin - 1e-12) size /= 2; }
    void doubleUp() { if (size * 2 <= kMax + 1e-12) size *= 2; }
    // Any size (typed); clamped to the allowed range.
    void setSize(double s) { size = std::clamp(s, kMin, kMax); }

    double snapValue(double v) const { return snap ? std::round(v / size) * size : v; }

    // Step actually drawn for a view showing `pxPerUnit` screen pixels per cm: the grid
    // size, doubled until lines are at least `minPx` apart (so dense grids stay readable).
    double drawnStep(double pxPerUnit, double minPx = 6) const {
        double s = size;
        while (s * pxPerUnit < minPx && s < 1e7) s *= 2;
        return s;
    }
};

} // namespace mazz
