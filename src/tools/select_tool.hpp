#pragma once
// Select tool (Shift+S), ARCHITECTURE §6 "Brush feel" and Selection:
//  - a click picks a whole piece, or one point / edge / face in those modes (2D: stacked
//    points together; clicking again on the same spot steps to the next one
//    underneath; 3D: the one in front); Cmd+click adds or removes; a drag on empty
//    space draws a rectangle (2D sees through, 3D only what's visible); an empty click
//    selects nothing; double-click an edge for its loop, Shift+double-click its ring.
//  - the selection box: always-visible handles in the 2D views; drag inside moves
//    (snapped), Shift+drag copies, a side or corner handle pushes those sides (the
//    opposite ones stay), Option+drag a corner rotates about the centre (15° steps,
//    +Shift free); X/Y/Z rings in the 3D view rotate too. Each is one undo step.
#include "tools/handles.hpp"
#include "tools/tool.hpp"
#include <map>
#include <vector>

namespace mazz {

class SelectTool : public Tool {
public:
    const char* name() const override { return "Select"; }
    void press(ToolEnv& env, const Pointer& p) override;
    const char* hint() const override;

    void drag(ToolEnv& env, const Pointer& p) override;
    void release(ToolEnv& env, const Pointer& p) override;
    void hover(ToolEnv& env, const Pointer& p) override;
    bool key(ToolEnv& env, int key, bool shift, bool cmd) override;
    void overlay(const ToolEnv& env, const ViewGeom& g, int view, ImDrawList* dl) const override;
    void cancel(ToolEnv& env) override;
    bool busy() const override { return action_ != Action::None; }

protected:
    enum class Action { None, Pending, Rect, Move, Resize, Rotate, Ring, Arrow };
    Action action_ = Action::None;
    int view_ = -1;
    double sx0_ = 0, sy0_ = 0, sx1_ = 0, sy1_ = 0; // press and current screen points
    double u0_ = 0, v0_ = 0;                      // press point (plane coordinates)
    bool shiftAtPress_ = false, cmdAtPress_ = false;
    int clicksAtPress_ = 1;
    bool insideAtPress_ = false;
    BoxGrab grab_;
    Vec3 mn0_, mx0_;                 // selection bounds at the press
    std::map<int, Vec3> start_;      // moved points and where they started
    int hotRing_ = -1;               // ring under the mouse in the 3D view
    int arrowAxis_ = -1;             // the arrow being dragged
    int hotArrow_ = -1, hotArrowView_ = -1; // arrow under the mouse

    // Clicking again on the same spot steps through the pieces underneath.
    double lastClickX_ = -1e9, lastClickY_ = -1e9;
    int lastClickView_ = -1;

    void beginEdit(ToolEnv& env, bool copy);
    void clickSelect(ToolEnv& env, const Pointer& p);
    void loopSelect(ToolEnv& env, const Pointer& p, bool ring);
};

} // namespace mazz
