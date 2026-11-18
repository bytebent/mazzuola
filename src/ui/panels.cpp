// Side panels, the texture view, and the small windows (Controls, About, grid size).
#include "app/editor.hpp"
#include "ui/theme.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

#ifndef MAZZ_VERSION
#define MAZZ_VERSION "dev"
#endif

namespace mazz {

namespace {
void dimText(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::kTextDim);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}
} // namespace

void Editor::textureView() {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 20 || avail.y < 20) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // An empty sheet: a square checkerboard (the "transparent" pattern) with a frame.
    const float side = std::floor(std::min(avail.x, avail.y) - 24);
    if (side > 16) {
        const ImVec2 p0{std::floor(origin.x + (avail.x - side) / 2), std::floor(origin.y + 12)};
        const int cells = 16;
        const float cell = side / cells;
        for (int y = 0; y < cells; ++y)
            for (int x = 0; x < cells; ++x) {
                const bool dark = (x + y) & 1;
                dl->AddRectFilled({p0.x + x * cell, p0.y + y * cell}, {p0.x + (x + 1) * cell, p0.y + (y + 1) * cell},
                                  dark ? IM_COL32(214, 216, 221, 255) : IM_COL32(236, 237, 240, 255));
            }
        dl->AddRect(p0, {p0.x + side, p0.y + side}, IM_COL32(170, 173, 180, 255));
        ImGui::SetCursorScreenPos({p0.x, p0.y + side + 10});
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + std::max(100.0f, side));
    dimText("No texture yet. Textures, the mapping tool and painting arrive in milestones 5 and 6.");
    ImGui::PopTextWrapPos();
}

void Editor::toolsPanel() {
    // Selection mode: four small toggles.
    ImGui::TextUnformatted("Select");
    const Act modes[4] = {Act::ModeVertex, Act::ModeEdge, Act::ModeFace, Act::ModePiece};
    const char* short4[4] = {"Vert", "Edge", "Face", "Piece"};
    const float bw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3) / 4;
    for (int k = 0; k < 4; ++k) {
        if (k) ImGui::SameLine();
        const bool on = (int)mode_ == k;
        if (on) {
            ImGui::PushStyleColor(ImGuiCol_Button, theme::kAccent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::kAccentHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::kAccentActive);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        }
        ImGui::PushID(k);
        if (ImGui::Button(short4[k], ImVec2(std::max(30.0f, bw), 0))) run(modes[k]);
        ImGui::PopID();
        if (on) ImGui::PopStyleColor(4);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (%s)", actionInfo(modes[k]).label, actionInfo(modes[k]).keys);
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Tools");
    const Act tools[] = {Act::ToolSelect, Act::ToolVertex, Act::ToolPrimitives, Act::ToolDraw, Act::ToolPushPull,
                         Act::ToolExtrude, Act::ToolCut, Act::ToolLatheLoft, Act::ToolBrush, Act::ToolMapping,
                         Act::ToolPaint, Act::ToolColour};
    for (Act a : tools) {
        const ActionInfo& info = actionInfo(a);
        const bool on = a == Act::ToolSelect && tool_ == Tool::Select;
        ImGui::BeginDisabled(!enabled(a));
        if (ImGui::Selectable(info.label, on)) run(a);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (info.milestone) ImGui::SetTooltip("%s - coming in milestone %d", displayKeys(info.keys).c_str(), info.milestone);
            else ImGui::SetTooltip("%s", displayKeys(info.keys).c_str());
        }
        const std::string keys = displayKeys(info.keys);
        ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(keys.c_str()).x);
        ImGui::TextDisabled("%s", keys.c_str());
    }
}

void Editor::propertiesPanel() {
    ImGui::SeparatorText("Selection");
    dimText("Nothing selected. Modelling arrives in milestone 1.");
    ImGui::SeparatorText("Grid");
    ImGui::Text("Size: %g cm", grid_.size);
    ImGui::SameLine();
    if (ImGui::SmallButton("-")) run(Act::GridSmaller);
    ImGui::SameLine();
    if (ImGui::SmallButton("+")) run(Act::GridLarger);
    ImGui::SameLine();
    if (ImGui::SmallButton("Set...")) run(Act::GridSet);
    bool show = grid_.visible, snap = grid_.snap;
    if (ImGui::Checkbox("Show grid", &show)) run(Act::ToggleGrid);
    if (ImGui::Checkbox("Snap to grid", &snap)) run(Act::ToggleSnap);
}

void Editor::partsPanel() { dimText("Parts (named groups of faces, with pivots) arrive in milestone 3."); }

void Editor::palettePanel() { dimText("The palette and saved palettes arrive in milestone 6."); }

void Editor::controlsWindow() {
    if (!controlsOpen_) return;
    ImGui::SetNextWindowSize(ImVec2(560, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Controls", &controlsOpen_)) {
        if (ImGui::BeginTable("##keys", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 3);
            ImGui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthStretch, 2);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 1.2f);
            ImGui::TableHeadersRow();
            auto row = [](const char* what, const std::string& keys, int milestone) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(what);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(keys.c_str());
                ImGui::TableNextColumn();
                if (milestone > 0) ImGui::TextDisabled("milestone %d", milestone);
            };
            for (int k = 0; k < (int)Act::Count; ++k) {
                const ActionInfo& info = actionInfo((Act)k);
                if (!*info.keys) continue;
                row(info.label, displayKeys(info.keys), info.milestone);
            }
            row("Pan a view", "middle mouse, or Space + left mouse", 0);
            row("Zoom a view", "mouse wheel (2D: toward the cursor)", 0);
            row("Orbit the 3D view", "right mouse drag", 0);
            row("Move while flying", "W A S D, Q/E down/up, Shift fast", 0);
            row("Resize the views", "drag the bars between them; double-click resets", 0);
            ImGui::EndTable();
        }
        ImGui::Spacing();
        dimText("Keys are read by physical position, so they work the same on any keyboard layout "
                "(\xC3\xA8 and + are the two keys right of P on an Italian keyboard).");
    }
    ImGui::End();
}

void Editor::aboutWindow() {
    if (!aboutOpen_) return;
    ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Appearing);
    if (ImGui::Begin("About Mazzuola", &aboutOpen_, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Mazzuola %s", MAZZ_VERSION);
        dimText("A small, fast modeller for static low-poly models. Units: 1 unit = 1 cm.");
        ImGui::Spacing();
        dimText("Uses GLFW, Dear ImGui, stb and the Roboto font (see docs/THIRD_PARTY.md).");
    }
    ImGui::End();
}

void Editor::gridSizeDialog() {
    if (gridDialogOpen_) {
        ImGui::OpenPopup("Set grid size");
        gridDialogOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Set grid size", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(140);
        ImGui::InputDouble("cm", &gridDialogValue_, 0, 0, "%g");
        ImGui::TextDisabled("%g to %g cm", Grid::kMin, Grid::kMax);
        const bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);
        if (ImGui::Button("Set (Return)") || enter) {
            grid_.setSize(gridDialogValue_);
            markViewsDirty();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel (Esc)") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

} // namespace mazz
