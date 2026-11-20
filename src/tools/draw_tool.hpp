#pragma once
// Draw tool (Shift+D), ARCHITECTURE §6 Draw (v0.2.2, agreed 2026-11-20): rectangles,
// circles and lines drawn onto one face cut it into new faces.
//  - Rectangle: click a corner on a face, click (or drag to) the opposite corner; it
//    lines up with the face's first edge. Then typed width / height or dragged corners.
//  - Circle: click the centre, click the radius; typed radius and segments (8).
//  - Line: click, click, ... Return; from an edge of the face to another.
//  - Return cuts it in (the shape ends up selected), Esc cancels; red when it doesn't fit.
//  - Snapping: grid, points (green), midpoints (cyan), on an edge (red), face centre
//    (blue); line pieces near X / Y / Z click onto them in the axis colour; Shift locks
//    the direction. No face highlight, only the snap marker.
#include "tools/tool.hpp"
#include <string>
#include <vector>

namespace mazz {

class DrawTool : public Tool {
public:
    enum class Shape { Rectangle, Circle, Line, Outline };
    enum class Snap { None, Grid, Point, Midpoint, Edge, Centre };

    const char* name() const override { return "Draw"; }
    const char* hint() const override;

    void press(ToolEnv& env, const Pointer& p) override;
    void drag(ToolEnv& env, const Pointer& p) override;
    void release(ToolEnv& env, const Pointer& p) override;
    void hover(ToolEnv& env, const Pointer& p) override;
    bool key(ToolEnv& env, int key, bool shift, bool cmd) override;
    void overlay(const ToolEnv& env, const ViewGeom& g, int view, ImDrawList* dl) const override;
    void properties(ToolEnv& env) override;
    void cancel(ToolEnv& env) override;
    bool busy() const override { return phase_ != Phase::Idle; }

    // Scripts and the typed fields.
    void setShape(ToolEnv& env, Shape s);
    void setSize(ToolEnv& env, double w, double h); // rectangle
    void setRadius(ToolEnv& env, double r);
    void setSegments(ToolEnv& env, int n);
    bool apply(ToolEnv& env); // Return
    bool drawing() const { return phase_ != Phase::Idle; }
    bool fits() const { return fits_; }
    Snap lastSnap() const { return cursor_.snap; }
    void setTwoSided(bool b) { twoSided_ = b; }
    int outlineLoops() const { return (int)holes_.size() + (outer_.empty() ? 0 : 1); }
    int lastAxis() const { return cursor_.axis; }

private:
    enum class Phase { Idle, Placing, Adjust, Line, Outline };
    struct Cursor {
        bool ok = false;
        Vec3 p;
        Snap snap = Snap::None;
        int axis = -1; // direction lock: 0 X, 1 Y, 2 Z
        int view = -1;
    };

    Shape shape_ = Shape::Rectangle;
    int segments_ = 8;
    Phase phase_ = Phase::Idle;
    int face_ = -1;
    Vec3 o_, u_, v_, n_;      // the face's frame
    Vec3 a_;                  // rectangle corner / circle centre / (line: unused)
    double w_ = 0, h_ = 0;    // rectangle size along u, v (signed)
    double r_ = 0;            // circle radius
    std::vector<Vec3> line_;  // line points so far
    Cursor cursor_;
    bool fits_ = true;
    std::string why_;
    int view_ = -1;
    double pressX_ = 0, pressY_ = 0;
    bool moved_ = false;      // the mouse moved far enough since the press (press-drag-release)
    int dragCorner_ = -1;     // Adjust: rectangle corner being dragged
    int lockAxis_ = -1;       // Shift held: this direction stays

    Cursor snapAt(const ToolEnv& env, const ViewGeom& g, double sx, double sy, bool shift) const;

    // Outline (v0.2.3): drawn on the 2D view's plane through the origin, facing you.
    std::vector<Vec3> current_;           // the loop being clicked
    std::vector<Vec3> outer_;             // closed outline
    std::vector<std::vector<Vec3>> holes_; // closed holes
    Vec3 planeN_;                         // towards the viewer
    bool twoSided_ = false;
    bool closes_ = false;                 // the cursor is on the loop's first point
    Cursor snapOutline(const ToolEnv& env, const ViewGeom& g, double sx, double sy, bool shift) const;
    void pressOutline(ToolEnv& env, const Pointer& p);
    void checkOutline(ToolEnv& env);
    bool applyOutline(ToolEnv& env);
    void drawOutline(const ViewGeom& g, int view, ImDrawList* dl) const;
    std::vector<Vec3> points() const;      // the shape so far (with the cursor while placing)
    void check(ToolEnv& env);              // fits_ / why_ from a trial cut
    void reset(ToolEnv& env);
    void alignFrame(const Mesh& m, int face); // u_ along X (or Y) on faces along an axis
};

} // namespace mazz
