// Side panels, the texture view, and the small windows (Controls, About, grid size).
#include "app/editor.hpp"
#include "platform/pen.hpp"
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

// The tool bar on the left (ARCHITECTURE §6): selection mode buttons at the top, then
// the tools that work in this release, as in Scalpellino.
void Editor::toolBar() {
    auto accentButton = [](const char* label, bool on, ImVec2 size) {
        if (on) {
            ImGui::PushStyleColor(ImGuiCol_Button, theme::kAccent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::kAccentHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::kAccentActive);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        }
        const bool clicked = ImGui::Button(label, size);
        if (on) ImGui::PopStyleColor(4);
        return clicked;
    };
    auto tip = [](Act a) {
        const ActionInfo& info = actionInfo(a);
        if (!ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) return;
        if (info.milestone) ImGui::SetTooltip("%s (%s) - coming later", info.label, displayKeys(info.keys).c_str());
        else ImGui::SetTooltip("%s (%s)", info.label, displayKeys(info.keys).c_str());
    };

    const float full = ImGui::GetContentRegionAvail().x;
    const float half = (full - ImGui::GetStyle().ItemSpacing.x) / 2;
    const Act modes[4] = {Act::ModeVertex, Act::ModeEdge, Act::ModeFace, Act::ModePiece};
    const char* names[4] = {"Vert", "Edge", "Face", "Piece"};
    for (int k = 0; k < 4; ++k) {
        if (k % 2) ImGui::SameLine();
        ImGui::PushID(k);
        ImGui::BeginDisabled(!enabled(modes[k]));
        if (accentButton(names[k], (int)doc_.sel.mode == k, ImVec2(half, 0))) run(modes[k]);
        ImGui::EndDisabled();
        tip(modes[k]);
        ImGui::PopID();
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    struct Entry { Act act; const char* label; const Tool* tool; };
    const Entry tools[] = {{Act::ToolSelect, "Select", &selectTool_},
                           {Act::ToolVertex, "Vertex", &vertexTool_},
                           {Act::ToolPrimitives, "Primitives", &primTool_}};
    for (const Entry& e : tools) {
        if (accentButton(e.label, tool_ == e.tool, ImVec2(full, ImGui::GetFrameHeight() * 1.2f))) run(e.act);
        tip(e.act);
    }
}

void Editor::propertiesPanel() {
    {
        ToolEnv env = toolEnv();
        tool_->properties(env);
    }
    ImGui::SeparatorText("Selection");
    if (doc_.sel.empty()) dimText("Nothing selected.");
    else {
        Vec3 mn, mx;
        selectionBounds(doc_.mesh, doc_.sel, mn, mx);
        ImGui::Text("%d faces", (int)doc_.sel.faces.size());
        ImGui::Text("size %g x %g x %g cm", mx.x - mn.x, mx.y - mn.y, mx.z - mn.z);
    }
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

void Editor::groupsPanel() { dimText("Groups (named sets of faces with their own pivot, Cmd+G) arrive in milestone 3."); }

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
            // Mouse gestures of the tools (ARCHITECTURE §6 "Brush feel").
            row("Draw a shape (Primitives)", "pick it in Properties; drag in a 2D view; handles adjust; Return creates, Esc cancels", 0);
            row("Select a piece", "click; click again on the same spot for the one underneath", 0);
            row("Add / remove a piece", "Cmd+click", 0);
            row("Select in a rectangle", "drag from empty space", 0);
            row("Move the selection", "drag inside its box (2D views)", 0);
            row("Copy the selection", "Shift+drag inside its box", 0);
            row("Resize", "drag a side or corner handle (the opposite side stays)", 0);
            row("Rotate", "Option+drag a corner (15\xC2\xB0 steps, +Shift free); rings in the 3D view", 0);
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

// A check for pointer and pen input: what ImGui sees, and what the pen reports. Click
// the button with a mouse and with the pen; both should count.
void Editor::penTestWindow() {
    if (!penTestOpen_) return;
    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_Appearing);
    if (ImGui::Begin("Pen and mouse test", &penTestOpen_, ImGuiWindowFlags_NoCollapse)) {
        static int clicks = 0;
        if (ImGui::Button("Click me with the mouse or the pen", ImVec2(-1, 40))) ++clicks;
        ImGui::Text("Clicks: %d", clicks);
        ImGui::Separator();
        const ImGuiIO& io = ImGui::GetIO();
        static const char* sources[] = {"mouse", "touch screen", "pen"};
        const int src = (int)io.MouseSource;
        ImGui::Text("Pointer: %.0f, %.0f (%s)", io.MousePos.x, io.MousePos.y, src >= 0 && src < 3 ? sources[src] : "?");
        ImGui::Text("Buttons: left %s, right %s, middle %s", io.MouseDown[0] ? "down" : "up",
                    io.MouseDown[1] ? "down" : "up", io.MouseDown[2] ? "down" : "up");
        const PenState& p = penState();
        ImGui::Text("Last event: %s%s", p.lastEvent, p.pen ? " (pen)" : "");
        ImGui::Text("Pen pressure: %.2f   tilt: %.2f, %.2f", p.pressure, p.tiltX, p.tiltY);
        ImGui::Text("Events: %llu   clicks moved to the pen tip: %llu", (unsigned long long)p.events,
                    (unsigned long long)p.positionFixes);
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
