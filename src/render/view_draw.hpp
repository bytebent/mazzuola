#pragma once
// One model view (2D orthographic or 3D): its camera(s), its offscreen image, and
// drawing its contents. Draws only when marked dirty.
#include "core/camera.hpp"
#include "core/grid.hpp"
#include "render/lines.hpp"
#include "render/view_target.hpp"

namespace mazz {

// View colours (light theme). Axis colours are the usual X red, Y green, Z blue.
namespace viewcol {
constexpr Rgba kBg2D{218, 220, 224, 255};
constexpr Rgba kBg3D{210, 213, 219, 255};
constexpr Rgba kMinor{200, 203, 209, 255};
constexpr Rgba kMajor{178, 182, 191, 255};
constexpr Rgba kAxis[3] = {{208, 62, 62, 255}, {58, 156, 74, 255}, {62, 102, 210, 255}};
// Selection in the views (pink, distinct from the X axis red and the UI accent).
constexpr Rgba kSelection{236, 92, 178, 255};
} // namespace viewcol

struct View {
    ViewDir dir = ViewDir::Top;
    OrthoCamera ortho;
    Camera3D cam3d;
    ViewTarget target;
    bool dirty = true;

    bool is3D() const { return dir == ViewDir::Persp; }
    // Changes direction; a 2D view keeps its zoom and moves its centre to match.
    void setDir(ViewDir d);
};

class ViewDrawer {
public:
    // Redraws `v` into its target if dirty or resized. Size in logical pixels and the
    // number of real pixels per logical pixel.
    void draw(View& v, const Grid& grid, double w, double h, double pxScale);

private:
    void build2D(const View& v, const Grid& grid, double w, double h);
    void build3D(const View& v, const Grid& grid);
    Batch batch_;
    BatchRenderer* renderer_ = nullptr; // created on first use (needs a GL context)
public:
    ~ViewDrawer();
};

} // namespace mazz
