// The operation windows (ARCHITECTURE §4 "operation with preview", §6 Transform,
// Multi-Replicate, Flatten): a small window that stays open over the app, its values
// shown live in the views through a live edit; Return applies it as one undo step, Esc
// (or closing the window) cancels. Only one is open at a time; while it is, the views
// can still be turned and zoomed but the tools don't take clicks.
#include "app/editor.hpp"
#include "core/select_ops.hpp"
#include "imgui.h"

namespace mazz {

namespace {
const char* opTitle(Editor::Op op) {
    switch (op) {
    case Editor::Op::Transform: return "Move / rotate / scale";
    case Editor::Op::Replicate: return "Multi-Replicate";
    case Editor::Op::Flatten: return "Flatten";
    default: return "";
    }
}

bool vec3Field(const char* label, Vec3& v, const char* fmt) {
    double a[3] = {v.x, v.y, v.z};
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 13);
    const bool changed = ImGui::InputScalarN(label, ImGuiDataType_Double, a, 3, nullptr, nullptr, fmt);
    if (changed) v = {a[0], a[1], a[2]};
    return changed;
}

bool pivotField(PivotKind& kind, Vec3& typed, bool travelling) {
    bool changed = false;
    const char* names[3] = {travelling ? "Selection (travels with the copies)" : "Selection centre", "World origin",
                            "Typed point"};
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 13);
    if (ImGui::BeginCombo("About", names[(int)kind])) {
        for (int k = 0; k < 3; ++k)
            if (ImGui::Selectable(names[k], (int)kind == k)) {
                kind = (PivotKind)k;
                changed = true;
            }
        ImGui::EndCombo();
    }
    if (kind == PivotKind::Typed) changed |= vec3Field("Point (cm)", typed, "%.3g");
    return changed;
}
} // namespace

void Editor::openOp(Op op) {
    cancelTool(); // also closes another operation window
    if (doc_.sel.empty()) {
        showMessage(opTitle(op), "Select something first.");
        return;
    }
    op_ = op;
    opSel_ = doc_.sel;
    // Transform values are added to where things are now, so they start at zero each
    // time (only the pivot choice is kept); Multi-Replicate keeps its pattern.
    if (op == Op::Transform) {
        const PivotKind pivot = opTransform_.pivot;
        const Vec3 typed = opTransform_.typed;
        opTransform_ = TransformParams();
        opTransform_.pivot = pivot;
        opTransform_.typed = typed;
    }
    doc_.beginLive();
    refreshOpPreview();
}

void Editor::refreshOpPreview() {
    if (op_ == Op::None) return;
    if (!doc_.liveActive()) doc_.beginLive();
    MeshEdit& e = doc_.live();
    e.revert();
    doc_.select(opSel_);
    switch (op_) {
    case Op::Transform: applyTransform(e, opSel_, opTransform_); break;
    case Op::Replicate: {
        const std::set<int> made = replicate(e, opSel_, opReplicate_);
        Selection copies;
        copies.mode = SelectMode::Face;
        copies.faces = made;
        doc_.select(convertSelection(doc_.mesh, copies, opSel_.mode)); // the copies end up selected
        break;
    }
    case Op::Flatten: flatten(e, opSel_, flattenAxis_, flattenTo_); break;
    case Op::None: break;
    }
    touch();
}

void Editor::applyOp() {
    if (op_ == Op::None) return;
    refreshOpPreview();
    const Op op = op_;
    op_ = Op::None;
    doc_.commitLive(op == Op::Transform ? "Transform" : op == Op::Replicate ? "Multi-Replicate" : "Flatten");
    touch();
}

void Editor::cancelOp() {
    if (op_ == Op::None) return;
    op_ = Op::None;
    doc_.cancelLive();
    touch();
}

void Editor::opWindow() {
    if (op_ == Op::None) return;
    // First time over the texture view (right of the four views), so the views and the
    // preview in them stay clear; it then stays where it was put.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({vp->WorkPos.x + vp->WorkSize.x * 0.615f, vp->WorkPos.y + vp->WorkSize.y * 0.06f},
                            ImGuiCond_FirstUseEver);
    bool open = true;
    const Op op = op_;
    if (!ImGui::Begin(opTitle(op), &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking)) {
        ImGui::End();
        if (!open) cancelOp();
        return;
    }
    bool changed = false;
    if (op == Op::Transform) {
        ImGui::TextDisabled("Added to where it is now.");
        changed |= vec3Field("Move (cm)", opTransform_.move, "%.3g");
        changed |= vec3Field("Rotate (degrees)", opTransform_.rotate, "%.3g");
        changed |= vec3Field("Scale (%)", opTransform_.scale, "%.4g");
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 13);
        changed |= ImGui::InputDouble("Along normal (cm)", &opTransform_.alongNormal, 0, 0, "%.3g");
        changed |= pivotField(opTransform_.pivot, opTransform_.typed, false);
        if (ImGui::SmallButton("Reset")) {
            opTransform_ = TransformParams();
            changed = true;
        }
    } else if (op == Op::Replicate) {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 13);
        if (ImGui::InputInt("Copies", &opReplicate_.copies)) {
            opReplicate_.copies = std::clamp(opReplicate_.copies, 1, 200);
            changed = true;
        }
        ImGui::TextDisabled("Each copy, one step further:");
        changed |= vec3Field("Offset (cm)", opReplicate_.offset, "%.3g");
        changed |= ImGui::Checkbox("Scale the offset too", &opReplicate_.scaleOffset);
        changed |= vec3Field("Turn (degrees)", opReplicate_.turn, "%.3g");
        changed |= vec3Field("Scale (%)", opReplicate_.scale, "%.4g");
        changed |= pivotField(opReplicate_.pivot, opReplicate_.typed, true);
    } else if (op == Op::Flatten) {
        ImGui::TextUnformatted("Line the points up on");
        for (int a = 0; a < 3; ++a) {
            if (a) ImGui::SameLine();
            const char* n[3] = {"X", "Y", "Z"};
            if (ImGui::RadioButton(n[a], flattenAxis_ == a)) {
                flattenAxis_ = a;
                changed = true;
            }
        }
        ImGui::TextUnformatted("at their");
        const char* to[3] = {"average", "lowest", "highest"};
        for (int k = 0; k < 3; ++k) {
            if (k) ImGui::SameLine();
            if (ImGui::RadioButton(to[k], (int)flattenTo_ == k)) {
                flattenTo_ = (FlattenTo)k;
                changed = true;
            }
        }
    }
    ImGui::Separator();
    const bool apply = ImGui::Button("Apply (Return)") || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                                                           (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                                                            ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)));
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel (Esc)");
    ImGui::End();
    if (changed) refreshOpPreview();
    if (apply) applyOp();
    else if (cancel || !open) cancelOp();
}

} // namespace mazz
