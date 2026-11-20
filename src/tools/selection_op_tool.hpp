#pragma once
// Tools that work on the selection with a live preview (Extrude, Inset; ARCHITECTURE §4
// "operation with preview"): while the tool is on and the selection suits it, the
// result is previewed with the settings in Properties; Return applies it (one undo
// step, the result selected), Esc drops it. Clicks and rectangles select as in the
// Select tool (never moving anything); a new selection or a changed setting brings the
// preview back.
#include "tools/pick_tool.hpp"
#include "tools/tool.hpp"
#include <string>

namespace mazz {

class SelectionOpTool : public Tool {
public:
    void press(ToolEnv& env, const Pointer& p) override;
    void drag(ToolEnv& env, const Pointer& p) override;
    void release(ToolEnv& env, const Pointer& p) override;
    void hover(ToolEnv& env, const Pointer& p) override { pick_.hover(env, p); }
    bool key(ToolEnv& env, int key, bool shift, bool cmd) override;
    void overlay(const ToolEnv& env, const ViewGeom& g, int view, ImDrawList* dl) const override;
    void update(ToolEnv& env) override;
    void activate(ToolEnv& env) override;
    void cancel(ToolEnv& env) override;
    bool busy() const override { return armed_ || picking_ || dragging_; }

    bool armed() const { return armed_; }
    bool valid() const { return valid_; }
    // Applies the preview (Return). False if there is none or it isn't possible.
    bool apply(ToolEnv& env);
    // A setting changed: preview again.
    void changed(ToolEnv& env);

protected:
    // The selection suits the tool (else the hint in Properties says what to select).
    virtual bool suits(const Mesh& m, const Selection& s) const = 0;
    // Builds the result into `e` from `source`; sets `result` (the new selection).
    virtual bool compute(MeshEdit& e, const Selection& source, Selection& result) = 0;
    virtual std::string stepName() const = 0;
    // Optional drag handle (Extrude's arrow): true if the press grabbed it.
    virtual bool pressHandle(ToolEnv&, const Pointer&) { return false; }
    virtual void dragHandle(ToolEnv&, const Pointer&) {}
    virtual void drawHandle(const ToolEnv&, const ViewGeom&, int, ImDrawList*) const {}
    // Apply / Cancel buttons and the "too wide" style message, for properties().
    void applyButtons(ToolEnv& env, const char* invalidText);
    // A field just finished with Return: apply.
    void applyOnReturn(ToolEnv& env);

    PickTool pick_;
    Selection source_;        // what the preview is computed from
    bool armed_ = false, valid_ = true;
    bool picking_ = false, dragging_ = false;
    bool wantArm_ = false;
    long seenSel_ = -1;       // selection version when the preview last stopped

    void arm(ToolEnv& env);
    void disarm(ToolEnv& env);
    void refresh(ToolEnv& env);
};

} // namespace mazz
