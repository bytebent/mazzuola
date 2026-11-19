#pragma once
// Primitives tool (Shift+B), ARCHITECTURE §6 Primitives, like Scalpellino's Block tool:
// drag a rectangle in a 2D view (snapped; the depth comes from the last shape made or
// selected), adjust the preview with the box handles in any 2D view or type its size in
// Properties, Return creates it (selected), Esc cancels. The shape and its settings are
// chosen in Properties (remembered between uses); round shapes stand along the depth
// axis of the view they were drawn in.
#include "core/primitives.hpp"
#include "tools/handles.hpp"
#include "tools/tool.hpp"
#include <optional>

namespace mazz {

class PrimTool : public Tool {
public:
    const char* name() const override { return "Primitives"; }
    const char* hint() const override;

    void press(ToolEnv& env, const Pointer& p) override;
    void drag(ToolEnv& env, const Pointer& p) override;
    void release(ToolEnv& env, const Pointer& p) override;
    bool key(ToolEnv& env, int key, bool shift, bool cmd) override;
    void overlay(const ToolEnv& env, const ViewGeom& g, int view, ImDrawList* dl) const override;
    void properties(ToolEnv& env) override;
    void cancel(ToolEnv& env) override;
    bool busy() const override { return box_.has_value() || mode_ != Mode::Idle; }

    bool hasPreview() const { return box_.has_value(); }
    int previewFaces() const;
    PrimParams& params() { return params_; }
    // Each shape keeps its own settings.
    void setShape(PrimKind k);
    // Creates the previewed box (Return). False if there is none.
    bool create(ToolEnv& env);

private:
    struct Box {
        Vec3 mn, mx;
    };
    enum class Mode { Idle, Drawing, Editing };
    Mode mode_ = Mode::Idle;
    std::optional<Box> box_;
    int view_ = -1;          // view of the current drag
    double u0_ = 0, v0_ = 0; // press point (plane coordinates; snapped when drawing)
    Vec3 anchor_;            // drawing: the snapped press point in the world
    Box start_{};            // editing: the box when the drag began
    BoxGrab grab_;
    PrimParams params_;       // the current shape's settings
    PrimParams saved_[kPrimKindCount]; // every shape's settings, remembered
    int axis_ = 2; // the shape stands along this world axis

    // The previewed shape, rebuilt only when the box or the settings change.
    mutable Mesh preview_;
    mutable bool previewValid_ = false;
    mutable Vec3 previewMn_, previewMx_;
    mutable PrimParams previewParams_;
    mutable int previewAxis_ = -1;
    mutable std::vector<Edge> previewEdges_;
    const Mesh& previewMesh() const;
};

} // namespace mazz
