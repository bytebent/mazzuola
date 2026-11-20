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
// In edge and point mode (point 6) it drags the edge or point under the cursor along
// the surface's normal there; the faces around tilt, no new faces.
#include "tools/tool.hpp"
#include <set>

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

    // What a push works on: a face, or the points of an edge or a single point.
    struct Target {
        int face = -1;       // face mode
        std::set<int> verts; // edge (2 points) or point (1) mode
        bool edge = false;
        bool valid() const { return face >= 0 || !verts.empty(); }
        bool operator==(const Target& o) const { return face == o.face && verts == o.verts; }
    };
    // The face a push would start on at this point (-1: none).
    static int faceAt(const Mesh& m, const ViewGeom& g, double sx, double sy);
    // What a push would start on here for the selection mode (edges, points, else faces).
    static Target targetAt(const Mesh& m, const ViewGeom& g, double sx, double sy, SelectMode mode);
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
    Target target_;        // what is pushed (indices in the model before the push)
    int moved_ = -1;       // face mode: where the face is in the preview
    std::set<int> region_, movedRegion_; // the faces pushed together, before / in the preview
    bool forceNew_ = false;
    int kind_ = 0;         // PushKind at the start (face mode)
    Vec3 origin_, normal_; // centre and normal before the push
    double distance_ = 0;
    double startDistance_ = 0, startT_ = 0; // at the press
    int view_ = -1;
    bool valid_ = true;
    Target hover_; // highlighted
    int hoverView_ = -1;

    void begin(ToolEnv& env, const Target& t, bool forceNew);
    void refresh(ToolEnv& env);
    // The target's centre and push direction; false if it has none.
    static bool frame(const Mesh& m, const Target& t, Vec3& origin, Vec3& normal);
};

} // namespace mazz
