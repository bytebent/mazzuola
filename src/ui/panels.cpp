// Side panels, the texture view, and the small windows (Controls, About, grid size).
#include "app/editor.hpp"
#include "platform/pen.hpp"
#include "ui/theme.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

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
        const bool on = (a == Act::ToolSelect && tool_ == Tool::Select) || (a == Act::ToolVertex && tool_ == Tool::Vertex) ||
                        (a == Act::ToolPrimitives && tool_ == Tool::Primitives);
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
    if (tool_ == Tool::Primitives) {
        primitivePanel();
    } else {
        ImGui::SeparatorText("Selection");
        Vec3 mn, mx;
        if (selectionBounds(mn, mx)) {
            const std::set<int> v = movingVerts();
            ImGui::Text("%zu points", v.size());
            const Vec3 c = (mn + mx) / 2, sz = mx - mn;
            ImGui::Text("Centre  %g, %g, %g cm", c.x, c.y, c.z);
            ImGui::Text("Size    %g x %g x %g cm", sz.x, sz.y, sz.z);
            if (ImGui::Button("Move / rotate / scale...")) run(Act::Transform);
            ImGui::SameLine();
            if (ImGui::Button("Move points...")) run(Act::MovePoints);
            if (tool_ == Tool::Select && !views_[0].is3D())
                dimText(handleMode_ == HandleMode::Scale ? "Handles: scale. Click the selection for rotate handles."
                                                         : "Handles: rotate. Click the selection for scale handles.");
        } else {
            dimText(doc_.model.mesh.faces.empty() ? "The model is empty. Create > Primitives, or the Primitives tool (Shift+B)."
                                                  : "Nothing selected. Click or drag a box in a view.");
        }
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

void Editor::primitivePanel() {
    ImGui::SeparatorText("Primitive");
    int k = (int)prim_.kind;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##primkind", primKindName(prim_.kind))) {
        for (int i = 0; i < kPrimKindCount; ++i)
            if (ImGui::Selectable(primKindName((PrimKind)i), k == i)) {
                prim_.kind = (PrimKind)i;
                if (primActive_) updatePrimPreview();
            }
        ImGui::EndCombo();
    }
    bool changed = false;
    auto num3 = [&](const char* label, Vec3& v) {
        double a[3] = {v.x, v.y, v.z};
        ImGui::SetNextItemWidth(-90);
        if (ImGui::InputScalarN(label, ImGuiDataType_Double, a, 3, nullptr, nullptr, "%g")) {
            v = {a[0], a[1], a[2]};
            changed = true;
        }
    };
    auto integer = [&](const char* label, int& v) {
        ImGui::SetNextItemWidth(110);
        if (ImGui::InputInt(label, &v)) changed = true;
    };
    auto real = [&](const char* label, double& v) {
        ImGui::SetNextItemWidth(110);
        if (ImGui::InputDouble(label, &v, 0, 0, "%g")) changed = true;
    };
    num3("Centre", prim_.centre);
    num3("Size", prim_.size);
    const PrimKind pk = prim_.kind;
    if (pk != PrimKind::Box && pk != PrimKind::Plane && pk != PrimKind::Rock) {
        ImGui::TextUnformatted("Stands along");
        for (int a = 0; a < 3; ++a) {
            ImGui::SameLine();
            static const char* n[3] = {"X##ax", "Y##ax", "Z##ax"};
            if (ImGui::RadioButton(n[a], prim_.axis == a)) { prim_.axis = a; changed = true; }
        }
    }
    switch (pk) {
    case PrimKind::Plane:
        integer("Rows", prim_.rows);
        integer("Columns", prim_.cols);
        if (ImGui::Checkbox("Flip diagonals", &prim_.flipDiagonals)) changed = true;
        break;
    case PrimKind::Box:
        integer("Segments X", prim_.segX);
        integer("Segments Y", prim_.segY);
        integer("Segments Z", prim_.segZ);
        real("Rounded edges (cm)", prim_.round);
        if (prim_.round > 0) integer("Rounding steps", prim_.roundSegs);
        break;
    case PrimKind::Cylinder:
    case PrimKind::Cone:
        integer("Sides", prim_.sides);
        integer("Segments", prim_.stacks);
        integer("Cap rings", prim_.capRings);
        break;
    case PrimKind::Tube:
        integer("Sides", prim_.sides);
        integer("Segments", prim_.stacks);
        real("Inner (0..1)", prim_.inner);
        break;
    case PrimKind::Torus:
        integer("Sides", prim_.sides);
        integer("Tube sides", prim_.rings);
        real("Inner (0..1)", prim_.inner);
        break;
    case PrimKind::Sphere:
        integer("Sides", prim_.sides);
        integer("Segments", prim_.stacks);
        break;
    case PrimKind::Geosphere:
        integer("Divisions", prim_.iterations);
        break;
    case PrimKind::Rock: {
        integer("Triangles", prim_.faces);
        real("Roughness %", prim_.roughness);
        num3("Stretch %", prim_.stretch);
        if (ImGui::Checkbox("Flat bottom", &prim_.flatBottom)) changed = true;
        if (prim_.flatBottom) real("Cut off %", prim_.bottomHeight);
        if (ImGui::Checkbox("Join into quads", &prim_.joinQuads)) changed = true;
        if (ImGui::Checkbox("Smooth", &prim_.smooth)) changed = true;
        int seed = (int)prim_.seed;
        ImGui::SetNextItemWidth(110);
        if (ImGui::InputInt("Seed", &seed)) { prim_.seed = (uint32_t)std::max(1, seed); changed = true; }
        ImGui::SameLine();
        if (ImGui::SmallButton("New seed")) newSeed();
        break;
    }
    }
    if (changed) {
        prim_ = clampParams(prim_);
        if (primActive_) updatePrimPreview();
    }
    ImGui::Spacing();
    if (!primActive_) {
        dimText("Drag a box in a 2D view, or press Show to start from these values.");
        if (ImGui::Button("Show")) startPrimitive(prim_.kind);
    } else {
        ImGui::Text("%d triangles", primPreview_.triangleCount());
        if (ImGui::Button("Create (Return)")) createPrimitive();
        ImGui::SameLine();
        if (ImGui::Button("Cancel (Esc)")) cancelPrimitive();
    }
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
            row("Select", displayKeys("click; Cmd+click adds or removes; drag a box from empty space"), 0);
            row("Move the selection (2D views)", "drag it", 0);
            row("Scale / rotate (2D views)", "drag the handles; click the selection to switch them", 0);
            row("Cancel a drag", "Esc", 0);
            row("Create the primitive shown", "Return (Esc cancels)", 0);
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
