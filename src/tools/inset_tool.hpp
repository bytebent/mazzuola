#pragma once
// Inset tool (Shift+I), ARCHITECTURE §6 (agreed 2026-11-20): a smaller copy of each
// selected face (or of the selection as one region) inside it, with a ring of new
// faces round it; typed width and an optional depth (the inner faces go in with
// straight walls, leaving a flat frame). Too wide: the faces show red and Return does
// nothing. The inner faces end up selected.
#include "core/model_ops.hpp"
#include "tools/selection_op_tool.hpp"

namespace mazz {

class InsetTool : public SelectionOpTool {
public:
    const char* name() const override { return "Inset"; }
    const char* hint() const override;
    void properties(ToolEnv& env) override;
    InsetParams& params() { return params_; }

protected:
    bool suits(const Mesh& m, const Selection& s) const override;
    bool compute(MeshEdit& e, const Selection& source, Selection& result) override;
    std::string stepName() const override { return "Inset"; }
    bool againAfterApply() const override { return false; }

private:
    InsetParams params_;
};

} // namespace mazz
