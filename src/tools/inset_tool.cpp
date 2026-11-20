#include "tools/inset_tool.hpp"
#include "core/numbers.hpp"

namespace mazz {

const char* InsetTool::hint() const {
    if (!armed_) return "select faces  click / rectangle: select";
    return "type the width (and depth)  Return: apply  Esc: cancel  click: select other faces";
}

bool InsetTool::suits(const Mesh&, const Selection& s) const {
    return (s.mode == SelectMode::Face || s.mode == SelectMode::Piece) && !s.faces.empty();
}

bool InsetTool::compute(MeshEdit& e, const Selection& source, Selection& result) {
    std::set<int> inner;
    if (!inset(e, source.faces, params_, &inner)) return false;
    result = Selection();
    result.mode = SelectMode::Face;
    result.faces = inner;
    return true;
}

void InsetTool::properties(ToolEnv& env) {
    ImGui::SeparatorText("Inset");
    if (!armed_ && !suits(env.doc.mesh, env.doc.sel)) ImGui::TextWrapped("Select faces to inset.");
    bool ch = false;
    const float fieldW = ImGui::GetFontSize() * 7;
    ImGui::SetNextItemWidth(fieldW);
    double w = params_.width;
    if (ImGui::InputDouble("Width (cm)", &w, 0, 0, "%g")) {
        params_.width = clean(w);
        ch = true;
    }
    applyOnReturn(env);
    ImGui::SetNextItemWidth(fieldW);
    double d = params_.depth;
    if (ImGui::InputDouble("Depth (cm)", &d, 0, 0, "%g")) {
        params_.depth = clean(d);
        ch = true;
    }
    applyOnReturn(env);
    dimWrapped("Depth: the inner faces go in (negative: out).");
    if (ImGui::RadioButton("Region", !params_.each)) params_.each = false, ch = true;
    ImGui::SameLine();
    if (ImGui::RadioButton("Each face", params_.each)) params_.each = true, ch = true;
    applyButtons(env, "Too wide for these faces.");
    if (ch) changed(env);
}

} // namespace mazz
