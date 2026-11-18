#include "core/grid_lines.hpp"
#include <algorithm>
#include <cmath>

namespace mazz {

namespace {
// Major every 8 drawn steps, counted from the origin.
GridLineKind kindFor(double value, double step) {
    const double i = std::round(value / step);
    if (std::fabs(value) < step * 1e-6) return GridLineKind::Minor; // caller marks axes
    return std::fmod(std::fabs(i), 8.0) < 0.5 ? GridLineKind::Major : GridLineKind::Minor;
}
} // namespace

std::vector<GridLine2D> gridLines2D(const Grid& grid, const OrthoCamera& cam, double w, double h) {
    std::vector<GridLine2D> out;
    double u0, v0, u1, v1;
    cam.screenToPlane(0, h, w, h, u0, v0); // bottom-left
    cam.screenToPlane(w, 0, w, h, u1, v1); // top-right
    const double step = grid.drawnStep(cam.zoom);
    if (grid.visible) {
        for (double u = std::floor(u0 / step) * step; u <= u1; u += step)
            if (std::fabs(u) > step * 1e-6) out.push_back({true, u, kindFor(u, step)});
        for (double v = std::floor(v0 / step) * step; v <= v1; v += step)
            if (std::fabs(v) > step * 1e-6) out.push_back({false, v, kindFor(v, step)});
    }
    // Axes are always shown, even with the grid hidden.
    if (u0 <= 0 && 0 <= u1) out.push_back({true, 0, GridLineKind::AxisV});
    if (v0 <= 0 && 0 <= v1) out.push_back({false, 0, GridLineKind::AxisU});
    return out;
}

std::vector<GridLine3D> gridLines3D(const Grid& grid, const Camera3D& cam, double& stepOut, double& extentOut) {
    std::vector<GridLine3D> out;
    // Lines roughly 1/60 of the viewing distance apart at least, never finer than the grid.
    const double dist = std::max(cam.distance, std::fabs(cam.eye().z));
    double step = grid.size;
    while (step < dist / 60 && step < 1e6) step *= 2;
    const double extent = step * 48;
    stepOut = step;
    extentOut = extent;
    // Centre on the grid point nearest to where the camera target sits on the ground.
    const double cx = std::round(cam.target.x / step) * step;
    const double cy = std::round(cam.target.y / step) * step;
    if (grid.visible) {
        for (int i = -48; i <= 48; ++i) {
            const double x = cx + i * step, y = cy + i * step;
            if (std::fabs(x) > step * 1e-6)
                out.push_back({{x, cy - extent, 0}, {x, cy + extent, 0}, kindFor(x, step)});
            if (std::fabs(y) > step * 1e-6)
                out.push_back({{cx - extent, y, 0}, {cx + extent, y, 0}, kindFor(y, step)});
        }
    }
    out.push_back({{cx - extent, 0, 0}, {cx + extent, 0, 0}, GridLineKind::AxisU});
    out.push_back({{0, cy - extent, 0}, {0, cy + extent, 0}, GridLineKind::AxisV});
    return out;
}

} // namespace mazz
