#pragma once
// Which grid lines a view should draw. Pure maths: the render layer turns the lines
// into colours and vertices.
#include "core/camera.hpp"
#include "core/grid.hpp"
#include <vector>

namespace mazz {

enum class GridLineKind { Minor, Major, AxisU, AxisV };

// A line in a 2D view's plane: constant u (vertical line) or constant v (horizontal).
struct GridLine2D {
    bool vertical;     // true: u = value, spans v; false: v = value, spans u
    double value;
    GridLineKind kind;
};

// Lines visible in a w x h (logical px) view. Every 8th drawn line is Major; the lines
// through the origin are AxisU (horizontal, the u axis) and AxisV (vertical).
std::vector<GridLine2D> gridLines2D(const Grid& grid, const OrthoCamera& cam, double w, double h);

// Ground-plane (z = 0) grid for the 3D view: lines of x = const and y = const within
// `extent` of the point under the camera target, step chosen from the camera distance.
struct GridLine3D {
    Vec3 a, b;
    GridLineKind kind; // AxisU = the x axis (y = 0), AxisV = the y axis (x = 0)
};
std::vector<GridLine3D> gridLines3D(const Grid& grid, const Camera3D& cam, double& stepOut, double& extentOut);

} // namespace mazz
