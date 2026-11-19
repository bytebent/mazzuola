#include "app/editor.hpp"
#include "ui/imgui_checks.hpp"
#include "ui/theme.hpp"
#include "imgui_internal.h" // DockBuilder, BeginViewportSideBar
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace mazz {

namespace {
// Window titles (also the ImGui IDs the dock layout refers to).
constexpr const char* kWinViews = "Views";
constexpr const char* kWinTexture = "Texture view";
constexpr const char* kWinTools = "Tools";
constexpr const char* kWinProps = "Properties";
constexpr const char* kWinParts = "Parts";
constexpr const char* kWinPalette = "Palette";
} // namespace

const char* selectModeName(SelectMode m) {
    switch (m) {
    case SelectMode::Vert: return "vertices";
    case SelectMode::Edge: return "edges";
    case SelectMode::Face: return "faces";
    case SelectMode::Piece: return "pieces";
    }
    return "?";
}

Editor::Editor(GLFWwindow* window, const EditorOptions& opt) : window_(window), opt_(opt) {
    // Default views: Top | Front over Right | 3D.
    views_[0].setDir(ViewDir::Top);
    views_[1].setDir(ViewDir::Front);
    views_[2].setDir(ViewDir::Right);
    views_[3].setDir(ViewDir::Persp);
    gridDialogValue_ = grid_.size;
    if (!opt_.scriptPath.empty()) {
        scriptMode_ = true;
        enableImGuiIdCheck();
        loadScript(opt_.scriptPath);
    }
}

Editor::~Editor() = default;

double Editor::viewScale() const {
    if (const char* s = std::getenv("MAZZ_VIEW_SCALE"); s && *s) {
        const double v = std::atof(s);
        if (v >= 0.5 && v <= 4) return v;
    }
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    return vp->FramebufferScale.x > 0 ? vp->FramebufferScale.x : 1.0;
}

std::string Editor::takeScreenshotRequest() {
    std::string s;
    s.swap(screenshotRequest_);
    return s;
}

void Editor::markViewsDirty() {
    for (View& v : views_) v.dirty = true;
}

int Editor::firstView3D() const {
    if (layout_.maximised >= 0 && views_[layout_.maximised].is3D()) return layout_.maximised;
    for (int i = 0; i < 4; ++i)
        if (views_[i].is3D()) return i;
    return -1;
}

void Editor::allViewsToOrigin() {
    for (View& v : views_) {
        v.ortho.cu = v.ortho.cv = 0;
        v.ortho.zoom = 2.0;
        v.cam3d = Camera3D{};
        v.dirty = true;
    }
}

// ---------------------------------------------------------------- actions

bool Editor::enabled(Act a) const {
    if (actionInfo(a).milestone != 0) return false;
    switch (a) {
    case Act::FlyMode: return flying_ || firstView3D() >= 0;
    case Act::MaximiseView: return layout_.maximised >= 0 || hoveredView_ >= 0;
    case Act::Undo: return doc_.canUndo();
    case Act::Redo: return doc_.canRedo();
    default: return true;
    }
}

void Editor::run(Act a) {
    if (!enabled(a)) return;
    switch (a) {
    case Act::Quit: requestQuit(); break;
    case Act::New: fileNew(); break;
    case Act::Open: fileOpen(); break;
    case Act::Save: fileSave(false); break;
    case Act::SaveAs: fileSave(true); break;
    case Act::Undo: if (doc_.undo()) touch(); break;
    case Act::Redo: if (doc_.redo()) touch(); break;
    // Points, edges and faces arrive in v0.1.2 (they convert the selection then).
    case Act::ModePiece: {
        Selection s = doc_.sel;
        s.mode = SelectMode::Piece;
        doc_.select(s);
        touch();
        break;
    }
    case Act::ToolSelect: tool_ = Tool::Select; break;
    case Act::GridSmaller: grid_.halve(); markViewsDirty(); break;
    case Act::GridLarger: grid_.doubleUp(); markViewsDirty(); break;
    case Act::GridSet: gridDialogValue_ = grid_.size; gridDialogOpen_ = true; break;
    case Act::ToggleGrid: grid_.visible = !grid_.visible; markViewsDirty(); break;
    case Act::ToggleSnap: grid_.snap = !grid_.snap; break;
    case Act::FlyMode: {
        if (flying_) { setFlying(false, -1); break; }
        int v = (hoveredView_ >= 0 && views_[hoveredView_].is3D()) ? hoveredView_ : firstView3D();
        if (v >= 0) setFlying(true, v);
        break;
    }
    case Act::OrbitOrigin:
        orbitSelection_ = false;
        for (View& v : views_)
            if (v.is3D()) {
                v.cam3d.distance = length(v.cam3d.eye() - Vec3{});
                v.cam3d.target = {};
                v.dirty = true;
            }
        break;
    case Act::OrbitSelection:
        // Nothing can be selected yet (milestone 1); until then this orbits the origin.
        orbitSelection_ = true;
        break;
    case Act::BackToOrigin: allViewsToOrigin(); break;
    case Act::MaximiseView:
        if (layout_.maximised >= 0) layout_.maximised = -1;
        else if (hoveredView_ >= 0) layout_.maximised = hoveredView_;
        markViewsDirty();
        break;
    case Act::ResetLayout:
        resetLayout_ = true;
        layout_ = ViewsLayout{};
        markViewsDirty();
        break;
    case Act::PaintWindow: paintWanted_ = !paintWanted_; break;
    case Act::Controls: controlsOpen_ = !controlsOpen_; break;
    case Act::About: aboutOpen_ = true; break;
    default: break;
    }
}

void Editor::keyPressed(int glfwKey, bool shift, bool cmd) {
    Chord c;
    c.key = glfwKey == GLFW_KEY_KP_ENTER ? GLFW_KEY_ENTER : glfwKey == GLFW_KEY_DELETE ? GLFW_KEY_BACKSPACE : glfwKey;
    c.shift = shift;
    c.cmd = cmd;
    keyQueue_.push_back(c);
}

void Editor::handleKeys() {
    const ImGuiIO& io = ImGui::GetIO();
    while (!keyQueue_.empty()) {
        const Chord c = keyQueue_.front();
        keyQueue_.pop_front();
        if (io.WantTextInput) continue; // typing in a field
        if (c.key == GLFW_KEY_ESCAPE && flying_ && !c.shift && !c.cmd) { setFlying(false, -1); continue; }
        // Fly mode (Z) only over a 3D view, or to leave it.
        if (c == actionChord(Act::FlyMode) && !flying_ && !(hoveredView_ >= 0 && views_[hoveredView_].is3D()))
            continue;
        if (flying_ && !c.cmd && (c.key == GLFW_KEY_W || c.key == GLFW_KEY_A || c.key == GLFW_KEY_S ||
                                  c.key == GLFW_KEY_D || c.key == GLFW_KEY_Q || c.key == GLFW_KEY_E))
            continue; // movement keys belong to the camera while flying
        for (int i = 0; i < (int)Act::Count; ++i) {
            const Act a = (Act)i;
            if (actionChord(a) == c) {
                run(a);
                break;
            }
        }
    }
}

// ---------------------------------------------------------------- fly mode

void Editor::setFlying(bool on, int view) {
    if (on == flying_) return;
    flying_ = on;
    flyView_ = on ? view : -1;
    if (!scriptMode_) glfwSetInputMode(window_, GLFW_CURSOR, on ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    flySkipLook_ = true;
}

void Editor::updateFly(double dt) {
    if (!flying_ || flyView_ < 0) return;
    View& v = views_[flyView_];
    if (!v.is3D()) { setFlying(false, -1); return; }
    const ImGuiIO& io = ImGui::GetIO();
    if (!flySkipLook_ && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0) && io.MouseDelta.x > -1e5f)
        v.cam3d.look(-io.MouseDelta.x * 0.2, io.MouseDelta.y * 0.2);
    flySkipLook_ = false;
    auto down = [&](int k) { return glfwGetKey(window_, k) == GLFW_PRESS; };
    const bool fast = down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT);
    const double speed = std::max(20.0, v.cam3d.distance * 0.8) * (fast ? 3.0 : 1.0) * dt;
    const double f = (down(GLFW_KEY_W) ? 1 : 0) - (down(GLFW_KEY_S) ? 1 : 0);
    const double r = (down(GLFW_KEY_D) ? 1 : 0) - (down(GLFW_KEY_A) ? 1 : 0);
    const double u = (down(GLFW_KEY_E) ? 1 : 0) - (down(GLFW_KEY_Q) ? 1 : 0);
    if (f || r || u) v.cam3d.fly(f * speed, r * speed, u * speed);
    v.dirty = true;
}

// ---------------------------------------------------------------- frame

bool Editor::frame(double dt) {
    if (scriptMode_) stepScript();
    handleKeys();
    updateFly(dt);

    menuBar();
    statusBar();
    dockspace();

    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(kWinViews, nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse))
        viewsPanel();
    ImGui::End();
    if (ImGui::Begin(kWinTexture, nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) textureView();
    ImGui::End();
    if (ImGui::Begin(kWinTools)) toolsPanel();
    ImGui::End();
    if (ImGui::Begin(kWinProps)) propertiesPanel();
    ImGui::End();
    if (ImGui::Begin(kWinParts)) partsPanel();
    ImGui::End();
    if (ImGui::Begin(kWinPalette)) palettePanel();
    ImGui::End();

    fileDialogs();
    updateTitle();
    controlsWindow();
    aboutWindow();
    gridSizeDialog();
    penTestWindow();
    if (scriptMode_) {
        for (const std::string& dup : takeImGuiIdConflicts()) scriptFail("conflicting widget ID: " + dup);
    }
    if (++startupFrames_ == 4 && !opt_.screenshot.empty()) {
        screenshotRequest_ = opt_.screenshot;
    } else if (startupFrames_ == 5 && !opt_.screenshot.empty()) {
        quit_ = true;
    }
    // A scripted run is busy only while commands are left (a finished script without
    // `quit` leaves the app idle, for the idle check).
    const bool scriptBusy = scriptMode_ && !scriptDone_;
    return flying_ || draggingView_ >= 0 || scriptBusy || !screenshotRequest_.empty() || !opt_.screenshot.empty();
}

void Editor::dockspace() {
    const ImGuiID id = ImGui::GetID("MainDock");
    if (resetLayout_ || ImGui::DockBuilderGetNode(id) == nullptr) {
        resetLayout_ = false;
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::DockBuilderRemoveNode(id);
        ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(id, vp->WorkSize);
        ImGuiID left, right;
        ImGui::DockBuilderSplitNode(id, ImGuiDir_Right, 0.17f, &right, &left);
        ImGuiID views, texture;
        ImGui::DockBuilderSplitNode(left, ImGuiDir_Right, 0.30f, &texture, &views);
        ImGuiID tools, rest, props, bottom;
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.42f, &tools, &rest);
        ImGui::DockBuilderSplitNode(rest, ImGuiDir_Up, 0.55f, &props, &bottom);
        ImGui::DockBuilderDockWindow(kWinViews, views);
        ImGui::DockBuilderDockWindow(kWinTexture, texture);
        ImGui::DockBuilderDockWindow(kWinTools, tools);
        ImGui::DockBuilderDockWindow(kWinProps, props);
        ImGui::DockBuilderDockWindow(kWinParts, bottom);
        ImGui::DockBuilderDockWindow(kWinPalette, bottom);
        if (ImGuiDockNode* n = ImGui::DockBuilderGetNode(views)) n->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
        ImGui::DockBuilderFinish(id);
    }
    ImGui::DockSpaceOverViewport(id, ImGui::GetMainViewport());
}

// ---------------------------------------------------------------- menus

void Editor::menuItem(Act a, bool selected) {
    const ActionInfo& info = actionInfo(a);
    const std::string keys = displayKeys(info.keys);
    if (ImGui::MenuItem(info.label, keys.empty() ? nullptr : keys.c_str(), selected, enabled(a))) run(a);
    if (info.milestone != 0 && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Coming in milestone %d", info.milestone);
}

void Editor::plannedItem(const char* label, int milestone) {
    ImGui::MenuItem(label, nullptr, false, false);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (milestone > 0) ImGui::SetTooltip("Coming in milestone %d", milestone);
        else ImGui::SetTooltip("Planned for later");
    }
}

void Editor::menuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        menuItem(Act::New);
        menuItem(Act::Open);
        ImGui::Separator();
        menuItem(Act::Save);
        menuItem(Act::SaveAs);
        if (ImGui::BeginMenu("Export", false)) ImGui::EndMenu();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Coming in milestone 1");
        ImGui::Separator();
        menuItem(Act::Quit);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        menuItem(Act::Undo);
        menuItem(Act::Redo);
        ImGui::Separator();
        menuItem(Act::Duplicate);
        menuItem(Act::Delete);
        ImGui::Separator();
        menuItem(Act::SelectAll);
        menuItem(Act::SelectNone);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Select")) {
        menuItem(Act::ModeVertex, doc_.sel.mode == SelectMode::Vert);
        menuItem(Act::ModeEdge, doc_.sel.mode == SelectMode::Edge);
        menuItem(Act::ModeFace, doc_.sel.mode == SelectMode::Face);
        menuItem(Act::ModePiece, doc_.sel.mode == SelectMode::Piece);
        ImGui::Separator();
        plannedItem("Grow selection", 1);
        plannedItem("Shrink selection", 1);
        plannedItem("Select similar", 1);
        plannedItem("Edge loop / ring", 1);
        ImGui::Separator();
        menuItem(Act::Hide);
        menuItem(Act::HideOthers);
        menuItem(Act::ShowAll);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Tools")) {
        for (Act a : {Act::ToolSelect, Act::ToolVertex, Act::ToolPrimitives, Act::ToolDraw, Act::ToolPushPull,
                      Act::ToolExtrude, Act::ToolCut, Act::ToolLatheLoft, Act::ToolBrush, Act::ToolMapping,
                      Act::ToolPaint, Act::ToolColour})
            menuItem(a, a == Act::ToolSelect && tool_ == Tool::Select);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Create")) {
        if (ImGui::BeginMenu("Primitives")) {
            for (const char* p : {"Plane", "Box", "Cylinder", "Cone", "Tube", "Torus", "Sphere", "Geosphere"})
                plannedItem(p, 1);
            ImGui::EndMenu();
        }
        plannedItem("Rock...", 1);
        ImGui::Separator();
        plannedItem("Lathe", 3);
        plannedItem("Loft", 3);
        ImGui::Separator();
        plannedItem("Image card...", 5);
        plannedItem("Tree...", 0);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Mesh")) {
        menuItem(Act::Transform);
        menuItem(Act::MovePoints);
        menuItem(Act::SnapToGrid);
        plannedItem("Mirror / mirror clone", 1);
        plannedItem("Flatten X / Y / Z", 1);
        ImGui::Separator();
        plannedItem("Extrude, inset, bevel, bridge, shell", 2);
        plannedItem("Divide, cut, slide, merge, weld", 2);
        plannedItem("Mirror mode", 2);
        ImGui::Separator();
        plannedItem("Parts, smoothing groups", 3);
        menuItem(Act::Problems);
        ImGui::Separator();
        plannedItem("Jitter points...", 4);
        plannedItem("Scatter on faces...", 7);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Texture")) {
        menuItem(Act::ApplyTexture);
        plannedItem("New / load texture...", 5);
        plannedItem("Export layout image...", 5);
        plannedItem("Generate texture...", 6);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Paint")) {
        plannedItem("Pixel painting", 6);
        plannedItem("Vertex colours", 7);
        plannedItem("Detach paint view", 8);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        menuItem(Act::MaximiseView, layout_.maximised >= 0);
        menuItem(Act::ResetLayout);
        menuItem(Act::PaintWindow, paintWanted_);
        ImGui::Separator();
        menuItem(Act::GridSmaller);
        menuItem(Act::GridLarger);
        menuItem(Act::GridSet);
        menuItem(Act::ToggleGrid, grid_.visible);
        menuItem(Act::ToggleSnap, grid_.snap);
        ImGui::Separator();
        menuItem(Act::OrbitOrigin, !orbitSelection_);
        menuItem(Act::OrbitSelection, orbitSelection_);
        menuItem(Act::FlyMode, flying_);
        ImGui::Separator();
        menuItem(Act::FrameSelection);
        menuItem(Act::BackToOrigin);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        menuItem(Act::Controls, controlsOpen_);
        menuItem(Act::About);
        ImGui::Separator();
        if (ImGui::MenuItem("Pen and mouse test", nullptr, penTestOpen_)) penTestOpen_ = !penTestOpen_;
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void Editor::statusBar() {
    const ImGuiWindowFlags f = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
    const float h = ImGui::GetFrameHeight();
    if (ImGui::BeginViewportSideBar("##status", ImGui::GetMainViewport(), ImGuiDir_Down, h, f)) {
        if (ImGui::BeginMenuBar()) {
            ImGui::TextUnformatted("Select tool");
            ImGui::TextDisabled("|");
            if (doc_.sel.empty()) ImGui::Text("%s, nothing selected", selectModeName(doc_.sel.mode));
            else ImGui::Text("%s, %d faces selected", selectModeName(doc_.sel.mode), (int)doc_.sel.faces.size());
            ImGui::TextDisabled("|");
            if (flying_) {
                ImGui::TextUnformatted("Flying: WASD, Q/E down/up, Shift fast, Z or Esc to stop");
            } else if (haveCursorWorld_ && cursorView_ >= 0) {
                const OrthoAxes ax = orthoAxes(views_[cursorView_].dir);
                static const char* names[] = {"x", "y", "z"};
                const int a0 = std::min(ax.u, ax.v), a1 = std::max(ax.u, ax.v);
                ImGui::Text("%s %.2f  %s %.2f cm", names[a0], cursorWorld_[a0], names[a1], cursorWorld_[a1]);
            } else {
                ImGui::TextDisabled("-");
            }
            ImGui::TextDisabled("|");
            ImGui::Text("grid %g cm%s%s", grid_.size, grid_.snap ? ", snap" : "", grid_.visible ? "" : ", hidden");
            int tris = 0;
            for (const Face& fc : doc_.mesh.faces) tris += fc.n - 2;
            char countBuf[96];
            std::snprintf(countBuf, sizeof countBuf, "%d vertices, %d triangles", (int)doc_.mesh.verts.size(), tris);
            const char* count = countBuf;
            const float w = ImGui::CalcTextSize(count).x;
            const float x = ImGui::GetWindowWidth() - w - ImGui::GetStyle().WindowPadding.x - 4;
            if (x > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(x);
            ImGui::TextUnformatted(count);
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
}

} // namespace mazz
