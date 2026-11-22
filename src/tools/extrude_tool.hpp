#pragma once
// Extrude tool (Shift+E), ARCHITECTURE §6 (agreed 2026-11-20): the selected faces (as
// one region or each on its own, along their normals or straight along X, Y or Z,
// optionally keeping the original), or selected open edges (straight, a strip of new
// faces). Live preview; an arrow to drag in the views; typed distance; Return applies
// and selects the new end faces (or edges).
// v0.2.6 (agreed 2026-11-22): Bevel shares the tool. Edges with faces on both sides, or
// points, switch it to Bevel by themselves; typed width and 1-3 segments; the arrow
// points into the surface and sets the width; red when too wide.
#include "core/bevel_ops.hpp"
#include "core/model_ops.hpp"
#include "tools/selection_op_tool.hpp"

namespace mazz {

class ExtrudeTool : public SelectionOpTool {
public:
    const char* name() const override { return "Extrude"; }
    const char* hint() const override;
    void properties(ToolEnv& env) override;
    ExtrudeParams& params() { return params_; }
    BevelParams& bevelParams() { return bevel_; }
    bool bevelling() const { return bevelling_; }
    // Extrude or Bevel, as the radio buttons (scripts).
    void setBevel(ToolEnv& env, bool on);

    // The arrow (for scripts): where it starts and which way it points.
    bool arrow(const Mesh& m, Vec3& origin, Vec3& dir) const;

protected:
    bool suits(const Mesh& m, const Selection& s) const override;
    bool compute(MeshEdit& e, const Selection& source, Selection& result) override;
    std::string stepName() const override { return "Extrude"; }
    void picked(const Mesh& m, const Selection& s) override;
    bool pressHandle(ToolEnv& env, const Pointer& p) override;
    void dragHandle(ToolEnv& env, const Pointer& p) override;
    void drawHandle(const ToolEnv& env, const ViewGeom& g, int view, ImDrawList* dl) const override;

private:
    ExtrudeParams params_;
    BevelParams bevel_;
    bool bevelling_ = false;
    std::string why_; // why the preview can't be applied
    bool canExtrude(const Mesh& m, const Selection& s) const;
    bool canBevel(const Mesh& m, const Selection& s) const;
    double& handleValue() { return bevelling_ ? bevel_.width : params_.distance; }
    double handleValue() const { return bevelling_ ? bevel_.width : params_.distance; }
    bool arrowOk_ = false;
    Vec3 arrowOrigin_, arrowDir_;
    bool arrowFor(const Mesh& m, const Selection& s, Vec3& origin, Vec3& dir) const;
    double startDistance_ = 0, startT_ = 0;
    int dragView_ = -1;
    Vec3 dragOrigin_, dragDir_;
    bool edges(const Selection& s) const { return s.mode == SelectMode::Edge; }
};

} // namespace mazz
