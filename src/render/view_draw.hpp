#pragma once
// One model view (2D orthographic or 3D): its camera(s), its offscreen image, and
// drawing its contents (grid, model, selection, previews, reference images, scale
// figure). Draws only when something it shows changed.
#include "core/camera.hpp"
#include "core/document.hpp"
#include "core/grid.hpp"
#include "core/ops.hpp"
#include "render/lines.hpp"
#include "render/view_target.hpp"
#include <cstdint>
#include <string>

namespace mazz {

class TextureCache;
class LitRenderer;
class ImageRenderer;

// View colours (light theme). Axis colours are the usual X red, Y green, Z blue.
namespace viewcol {
constexpr Rgba kBg2D{218, 220, 224, 255};
constexpr Rgba kBg3D{210, 213, 219, 255};
constexpr Rgba kMinor{200, 203, 209, 255};
constexpr Rgba kMajor{178, 182, 191, 255};
constexpr Rgba kAxis[3] = {{208, 62, 62, 255}, {58, 156, 74, 255}, {62, 102, 210, 255}};
// Selection in the views: pink, distinct from the X axis red and the UI accent.
constexpr Rgba kSelection{236, 72, 170, 255};
constexpr Rgba kSelectionFill{236, 72, 170, 60};
// Model lines and faces.
constexpr Rgba kEdge2D{58, 60, 68, 255};
constexpr Rgba kWire3D{40, 42, 50, 150};
constexpr Rgba kFace{206, 207, 212, 255};
// What a creation tool is about to make (the preview), before Return.
constexpr Rgba kPreview{0, 140, 190, 255};
constexpr Rgba kFigure{120, 110, 150, 255};
} // namespace viewcol

// What the views show. `version` changes whenever any of it changes.
struct Scene {
    const Model* model = nullptr;
    const Selection* sel = nullptr;
    SelectMode mode = SelectMode::Face;
    const Mesh* preview = nullptr;  // creation preview (drawn in kPreview)
    bool showFigure = false;
    Vec3 figureAt;                  // where the scale figure's feet are
    bool wireOverlay = true;        // lines over the faces in the 3D view
    bool showRefs = true;
    TextureCache* textures = nullptr; // for reference images (main window only)
    std::string refBaseDir;         // reference image paths are relative to this
    uint64_t version = 0;
};

struct View {
    ViewDir dir = ViewDir::Top;
    OrthoCamera ortho;
    Camera3D cam3d;
    ViewTarget target;
    bool dirty = true;
    uint64_t drawnVersion = ~0ull;

    bool is3D() const { return dir == ViewDir::Persp; }
    // Changes direction; a 2D view keeps its zoom and moves its centre to match.
    void setDir(ViewDir d);
    // The view's matrix for picking and overlays.
    Mat4 viewProj(double w, double h) const;
};

// Resolves a reference image path against the model's folder.
std::string resolveRefPath(const std::string& path, const std::string& baseDir);

class ViewDrawer {
public:
    ViewDrawer();
    ~ViewDrawer();
    ViewDrawer(const ViewDrawer&) = delete;
    ViewDrawer& operator=(const ViewDrawer&) = delete;
    // Redraws `v` into its target if anything changed. Size in logical pixels and the
    // number of real pixels per logical pixel.
    void draw(View& v, const Grid& grid, const Scene& scene, double w, double h, double pxScale);

private:
    void build2D(const View& v, const Grid& grid, const Scene& scene, double w, double h);
    void build3D(const View& v, const Grid& grid, const Scene& scene);
    void drawRefs(const View& v, const Scene& scene, bool front, const Mat4& viewProj);
    Batch batch_;
    BatchRenderer* lines_ = nullptr;   // created on first use (needs a GL context)
    LitRenderer* lit_ = nullptr;
    ImageRenderer* images_ = nullptr;
};

} // namespace mazz
