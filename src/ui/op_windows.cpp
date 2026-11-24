// The operation windows (ARCHITECTURE §4 "operation with preview", §6 Transform,
// Multi-Replicate, Flatten): a small window that stays open over the app, its values
// shown live in the views through a live edit; Return applies it as one undo step, Esc
// (or closing the window) cancels. Only one is open at a time; while it is, the views
// can still be turned and zoomed but the tools don't take clicks.
#include "app/editor.hpp"
#include "core/bevel_ops.hpp"
#include "core/clean_ops.hpp"
#include "core/divide_ops.hpp"
#include "core/mirror_ops.hpp"
#include "core/model_ops.hpp"
#include "core/select_ops.hpp"
#include "imgui.h"
#include <cstdio>

namespace mazz {

namespace {
const char* opTitle(Editor::Op op) {
    switch (op) {
    case Editor::Op::Transform: return "Move / rotate / scale";
    case Editor::Op::Replicate: return "Multi-Replicate";
    case Editor::Op::Flatten: return "Flatten";
    case Editor::Op::Subdivide: return "Subdivide";
    case Editor::Op::DivideEdges: return "Divide edges";
    case Editor::Op::Poke: return "Poke";
    case Editor::Op::Weld: return "Weld by distance";
    case Editor::Op::Bridge: return "Bridge";
    case Editor::Op::Shell: return "Shell";
    case Editor::Op::Symmetric: return "Make symmetric";
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
    opWhy_.clear();
    if (op == Op::Symmetric) { // the whole model; the mirror mode's plane and side to start
        symAxis_ = mirrorPlane_;
        symKeep_ = doc_.mirrorSide;
    } else if (doc_.sel.empty()) {
        showMessage(opTitle(op), "Select something first.");
        return;
    }
    op_ = op;
    opSel_ = doc_.sel;
    opWasClosed_ = isClosed(doc_.mesh);
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
    case Op::Subdivide:
    case Op::Poke: {
        const std::set<int> faces = convertSelection(doc_.mesh, opSel_, SelectMode::Face).faces;
        std::set<int> out;
        opWhy_.clear();
        const bool ok = faces.empty() ? (opWhy_ = "select faces first", false)
                        : op_ == Op::Subdivide ? subdivide(e, faces, subdivN_, subdivM_, &out, &opWhy_)
                                               : poke(e, faces, pokeHeight_, &out);
        if (!ok) {
            if (opWhy_.empty()) opWhy_ = "it can't be done here";
            e.revert();
            break;
        }
        Selection s;
        s.mode = SelectMode::Face;
        s.faces = out;
        doc_.select(s);
        break;
    }
    case Op::DivideEdges: {
        const std::set<Edge> edges =
            opSel_.mode == SelectMode::Edge ? opSel_.edges : convertSelection(doc_.mesh, opSel_, SelectMode::Edge).edges;
        opWhy_.clear();
        if (edges.empty()) opWhy_ = "select edges first";
        else if (!divideEdges(e, edges, divideN_, &opWhy_)) {
            if (opWhy_.empty()) opWhy_ = "it can't be done here";
            e.revert();
        }
        break;
    }
    case Op::Bridge:
    case Op::Shell: {
        std::set<int> out;
        opWhy_.clear();
        bool ok;
        if (op_ == Op::Shell) {
            const std::set<int> faces = convertSelection(doc_.mesh, opSel_, SelectMode::Face).faces;
            ok = shell(e, faces, shell_, &out, &opWhy_);
        } else if (opSel_.mode == SelectMode::Edge) {
            ok = bridgeHoles(e, opSel_.edges, bridge_, &out, &opWhy_); // an edge on each hole
        } else {
            const std::set<int> faces = convertSelection(doc_.mesh, opSel_, SelectMode::Face).faces;
            ok = bridgeFaces(e, faces, bridge_, &out, &opWhy_);
        }
        if (!ok) {
            if (opWhy_.empty()) opWhy_ = "it can't be done here";
            e.revert();
            break;
        }
        Selection s;
        s.mode = SelectMode::Face;
        s.faces = out;
        doc_.select(s);
        break;
    }
    case Op::Weld: {
        const std::set<int> verts = convertSelection(doc_.mesh, opSel_, SelectMode::Vert).verts;
        opWhy_.clear();
        weldCount_ = weldByDistance(e, verts, weldDist_);
        if (weldCount_ > 0 && isClosed(doc_.mesh) != opWasClosed_) {
            // Welding that opens a closed model isn't wanted (it can close an open one).
            if (opWasClosed_) opWhy_ = "it would open the model";
        }
        if (weldCount_ == 0) opWhy_ = "no points are that close";
        if (!opWhy_.empty()) e.revert();
        else {
            Selection none;
            none.mode = SelectMode::Vert;
            doc_.select(none);
        }
        break;
    }
    case Op::Symmetric: {
        opWhy_.clear();
        if (!makeSymmetric(e, symAxis_, symKeep_, &opWhy_)) {
            if (opWhy_.empty()) opWhy_ = "it can't be done here";
            e.revert();
            break;
        }
        Selection none;
        none.mode = opSel_.mode;
        doc_.select(none);
        break;
    }
    case Op::None: break;
    }
    // Mirror mode: what the window moved on one side, mirrored (Make symmetric does its own).
    if (op_ != Op::Symmetric) doc_.mirrorFixLive();
    touch();
}

void Editor::applyOp() {
    if (op_ == Op::None) return;
    refreshOpPreview();
    if (!opWhy_.empty()) return; // can't: the window says why
    const Op op = op_;
    op_ = Op::None;
    doc_.commitLive(op == Op::Transform ? "Transform" : op == Op::Replicate ? "Multi-Replicate" : op == Op::Flatten ? "Flatten" : opTitle(op));
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
    } else if (op == Op::Subdivide) {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
        if (ImGui::InputInt("Pieces along", &subdivN_)) subdivN_ = std::clamp(subdivN_, 1, 16), changed = true;
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
        if (ImGui::InputInt("Pieces across", &subdivM_)) subdivM_ = std::clamp(subdivM_, 1, 16), changed = true;
        ImGui::TextDisabled("Along each face's first edge, then across.\nTriangles: pieces along, both ways.");
    } else if (op == Op::DivideEdges) {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
        if (ImGui::InputInt("Pieces", &divideN_)) divideN_ = std::clamp(divideN_, 2, 32), changed = true;
    } else if (op == Op::Poke) {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
        changed |= ImGui::InputDouble("Height (cm)", &pokeHeight_, 0, 0, "%g");
        ImGui::TextDisabled("The middle point goes out along the face's normal\n(negative: in).");
    } else if (op == Op::Weld) {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
        if (ImGui::InputDouble("Distance (cm)", &weldDist_, 0, 0, "%g")) weldDist_ = std::max(0.0, weldDist_), changed = true;
        ImGui::TextDisabled("Selected points this close become one.");
        if (opWhy_.empty()) ImGui::Text("%d point%s go.", weldCount_, weldCount_ == 1 ? "" : "s");
    } else if (op == Op::Bridge) {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
        if (ImGui::InputInt("Segments", &bridge_.segments)) bridge_.segments = std::clamp(bridge_.segments, 1, 32), changed = true;
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
        if (ImGui::InputInt("Twist", &bridge_.twist)) changed = true;
        ImGui::TextDisabled("Two faces or two blocks of faces: they go and a tube\njoins them. Edge mode: an edge on each of two holes.");
    } else if (op == Op::Shell) {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
        if (ImGui::InputDouble("Thickness (cm)", &shell_.thickness, 0, 0, "%g"))
            shell_.thickness = std::max(0.0, shell_.thickness), changed = true;
        const char* side[3] = {"Inward", "Outward", "Centred"};
        for (int k = 0; k < 3; ++k) {
            if (k) ImGui::SameLine();
            if (ImGui::RadioButton(side[k], (int)shell_.side == k)) shell_.side = (ShellSide)k, changed = true;
        }
        ImGui::TextDisabled("An open surface gets a wall this thick:\na turned-round copy and a rim.");
    } else if (op == Op::Symmetric) {
        ImGui::TextUnformatted("Plane");
        const char* pn[3] = {"X", "Y", "Z"};
        for (int a = 0; a < 3; ++a) {
            ImGui::SameLine();
            if (ImGui::RadioButton(pn[a], symAxis_ == a)) symAxis_ = a, changed = true;
        }
        ImGui::TextUnformatted("Keep the side");
        char lbl[2][16];
        std::snprintf(lbl[0], sizeof lbl[0], "+%s", pn[symAxis_]);
        std::snprintf(lbl[1], sizeof lbl[1], "-%s", pn[symAxis_]);
        ImGui::SameLine();
        if (ImGui::RadioButton(lbl[0], symKeep_ > 0)) symKeep_ = 1, changed = true;
        ImGui::SameLine();
        if (ImGui::RadioButton(lbl[1], symKeep_ < 0)) symKeep_ = -1, changed = true;
        ImGui::TextDisabled("The other side is rebuilt as its mirror; faces\nacross the plane are cut along it.");
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
    if (!opWhy_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.2f, 0.2f, 1));
        ImGui::TextUnformatted(("Can't: " + opWhy_ + ".").c_str());
        ImGui::PopStyleColor();
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
