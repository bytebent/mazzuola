#pragma once
// Push/pull tool (Shift+F), ARCHITECTURE §6 (agreed 2026-11-20):
//  - the face under the cursor lights up: any face in the 3D view; in a 2D view a face
//    seen edge-on (a line), dragged across the view like a brush side;
//  - press and drag moves it along its normal; the distance snaps to grid steps and can
//    be typed in Properties;
//  - Return applies, Esc cancels; releasing keeps the preview; pressing on another face
//    applies the pending push first;
//  - faces around it at right angles stretch, else new side faces; Cmd at the press
//    always makes new faces; a flat shape becomes a solid;
//  - only the face clicked, never the selection.
#include "tools/tool.hpp"

namespace mazz {

class PushPullTool : public Tool {
public:
    const char* name() const override { return "Push/pull"; }
    const char* hint() const override;

    void press(ToolEnv& env, const Pointer& p) override;
    void drag(ToolEnv& env, const Pointer& p) override;
    void release(ToolEnv& env, const Pointer& p) override;
    void hover(ToolEnv& env, const Pointer& p) override;
    bool key(ToolEnv& env, int key, bool shift, bool cmd) override;
    void overlay(const ToolEnv& env, const ViewGeom& g, int view, ImDrawList* dl) const override;
    void properties(ToolEnv& env) override;
    void cancel(ToolEnv& env) override;
    bool busy() const override { return state_ != State::Idle; }

    // The face a push would start on at this point (-1: none).
    static int faceAt(const Mesh& m, const ViewGeom& g, double sx, double sy);
    // Applies the pending push (Return). False if there is none or it isn't possible.
    bool apply(ToolEnv& env);
    // Scripts and the typed field: the pending push's distance.
    bool pending() const { return state_ != State::Idle; }
    double distance() const { return distance_; }
    bool valid() const { return valid_; }
    void setDistance(ToolEnv& env, double d);

private:
    enum class State { Idle, Dragging, Pending };
    State state_ = State::Idle;
    int face_ = -1;          // the face pushed (index in the model before the push)
    int moved_ = -1;         // where it is in the preview
    bool forceNew_ = false;
    int kind_ = 0;           // PushKind at the start
    Vec3 origin_, normal_;   // the face's centre and normal before the push
    double distance_ = 0;
    double startDistance_ = 0, startT_ = 0; // at the press
    int view_ = -1;
    bool valid_ = true;
    int hover_ = -1, hoverView_ = -1;       // highlighted face

    void begin(ToolEnv& env, int face, bool forceNew);
    void refresh(ToolEnv& env);
};

} // namespace mazz
