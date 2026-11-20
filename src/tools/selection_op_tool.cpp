#include "tools/selection_op_tool.hpp"
#include "core/select_ops.hpp"
#include <GLFW/glfw3.h>

namespace mazz {

void SelectionOpTool::activate(ToolEnv& env) {
    wantArm_ = true;
    update(env);
}

void SelectionOpTool::update(ToolEnv& env) {
    Document& doc = env.doc;
    if (armed_ || picking_ || dragging_ || doc.liveActive()) return;
    if (!wantArm_ && doc.selVersion() == seenSel_) return;
    wantArm_ = false;
    seenSel_ = doc.selVersion();
    if (suits(doc.mesh, doc.sel)) arm(env);
}

void SelectionOpTool::arm(ToolEnv& env) {
    Document& doc = env.doc;
    source_ = doc.sel;
    doc.beginLive();
    armed_ = true;
    refresh(env);
}

void SelectionOpTool::refresh(ToolEnv& env) {
    Document& doc = env.doc;
    if (!armed_) return;
    MeshEdit& e = doc.live();
    e.revert();
    Selection result = source_;
    valid_ = compute(e, source_, result);
    if (!valid_) {
        e.revert(); // shown as it was; Properties says why
        result = source_;
    }
    doc.select(result);
    env.touch();
}

void SelectionOpTool::disarm(ToolEnv& env) {
    if (!armed_) return;
    env.doc.cancelLive(); // the selection goes back to the source
    armed_ = false;
    valid_ = true;
    seenSel_ = env.doc.selVersion();
    env.touch();
}

void SelectionOpTool::changed(ToolEnv& env) {
    if (armed_) refresh(env);
    else {
        wantArm_ = true;
        update(env);
    }
}

bool SelectionOpTool::apply(ToolEnv& env) {
    if (!armed_ || !valid_) return false;
    env.doc.commitLive(stepName());
    armed_ = false;
    seenSel_ = env.doc.selVersion(); // the result stays selected, no new preview
    env.touch();
    return true;
}

void SelectionOpTool::cancel(ToolEnv& env) {
    if (picking_) pick_.cancel(env);
    picking_ = dragging_ = false;
    disarm(env);
    wantArm_ = false;
    seenSel_ = env.doc.selVersion();
}

void SelectionOpTool::press(ToolEnv& env, const Pointer& p) {
    if (pressHandle(env, p)) {
        dragging_ = true;
        return;
    }
    // Selecting: the preview goes, the click picks from the model as it is.
    disarm(env);
    picking_ = true;
    pick_.press(env, p);
}

void SelectionOpTool::drag(ToolEnv& env, const Pointer& p) {
    if (dragging_) dragHandle(env, p);
    else if (picking_) pick_.drag(env, p);
}

void SelectionOpTool::release(ToolEnv& env, const Pointer& p) {
    if (dragging_) {
        dragging_ = false;
        env.touch();
        return;
    }
    if (picking_) {
        pick_.release(env, p);
        picking_ = false;
        wantArm_ = true; // the (new) selection gets a preview
        update(env);
    }
}

bool SelectionOpTool::key(ToolEnv& env, int key, bool shift, bool cmd) {
    if (shift || cmd || !armed_) return false;
    if (key == GLFW_KEY_ENTER) {
        apply(env);
        return true;
    }
    if (key == GLFW_KEY_ESCAPE) {
        disarm(env);
        wantArm_ = false;
        return true;
    }
    return false;
}

void SelectionOpTool::overlay(const ToolEnv& env, const ViewGeom& g, int view, ImDrawList* dl) const {
    pick_.overlay(env, g, view, dl);
    // Not possible: the faces it would work on, outlined in red.
    if (armed_ && !valid_) {
        const Mesh& m = env.doc.mesh;
        const Selection f = convertSelection(m, source_, SelectMode::Face);
        for (int fi : f.faces) {
            if (fi >= (int)m.faces.size()) continue;
            const Face& face = m.faces[fi];
            ImVec2 pts[4];
            bool ok = true;
            for (int k = 0; k < face.n && ok; ++k) {
                double x, y;
                ok = g.project(m.verts[face.v[k]], x, y);
                pts[k] = ImVec2((float)x, (float)y);
            }
            if (ok) dl->AddPolyline(pts, face.n, IM_COL32(230, 60, 60, 255), ImDrawFlags_Closed, 2.5f);
        }
        for (const Edge& e : source_.edges) {
            double ax, ay, bx, by;
            if (g.project(m.verts[e.first], ax, ay) && g.project(m.verts[e.second], bx, by))
                dl->AddLine(ImVec2((float)ax, (float)ay), ImVec2((float)bx, (float)by), IM_COL32(230, 60, 60, 255), 2.5f);
        }
    }
    drawHandle(env, g, view, dl);
}

void SelectionOpTool::applyOnReturn(ToolEnv& env) {
    if (ImGui::IsItemDeactivatedAfterEdit() &&
        (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
        apply(env);
}

void SelectionOpTool::applyButtons(ToolEnv& env, const char* invalidText) {
    if (armed_ && !valid_) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.2f, 0.2f, 1));
        ImGui::TextWrapped("%s", invalidText);
        ImGui::PopStyleColor();
    }
    ImGui::BeginDisabled(!armed_ || !valid_);
    if (ImGui::Button("Apply (Return)")) apply(env);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!armed_);
    if (ImGui::Button("Cancel (Esc)")) {
        disarm(env);
        wantArm_ = false;
    }
    ImGui::EndDisabled();
}

} // namespace mazz
