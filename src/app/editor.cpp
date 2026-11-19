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
    case SelectMode::Vertex: return "vertices";
    case SelectMode::Edge: return "edges";
    case SelectMode::Face: return "faces";
    case SelectMode::Piece: return "pieces";
    }
    return "?";
}

const char* toolName(Tool t) {
    switch (t) {
    case Tool::Select: return "Select";
    case Tool::Vertex: return "Vertex";
    case Tool::Primitives: return "Primitives";
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
    if (!opt_.openPath.empty()) openPath(opt_.openPath);
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

Scene Editor::scene(bool withRefs) {
    Scene s;
    s.model = &doc_.model;
    s.sel = &doc_.sel;
    s.mode = mode_;
    s.preview = primActive_ ? &primPreview_ : nullptr;
    s.showFigure = showFigure_;
    s.figureAt = figurePosition();
    s.wireOverlay = wireOverlay_;
    s.showRefs = showRefs_ && withRefs;
    s.textures = withRefs ? &textures_ : nullptr;
    s.refBaseDir = modelDir();
    s.version = sceneVersion_;
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

void Editor::frameSelection() {
    Vec3 mn, mx;
    if (!selectionBounds(mn, mx) && !doc_.model.mesh.bounds(mn, mx)) return;
    const Vec3 c = (mn + mx) / 2;
    const double size = std::max(8.0, length(mx - mn));
    for (int i = 0; i < 4; ++i) {
        View& v = views_[i];
        if (v.is3D()) {
            v.cam3d.target = c;
            v.cam3d.distance = std::clamp(size * 1.4, Camera3D::kMinDistance, Camera3D::kMaxDistance);
        } else {
            v.ortho.worldToPlane(c, v.ortho.cu, v.ortho.cv);
            const double w = std::max(50.0, (double)viewImgSize_[i].x), h = std::max(50.0, (double)viewImgSize_[i].y);
            double u0, v0, u1, v1;
            v.ortho.worldToPlane(mn, u0, v0);
            v.ortho.worldToPlane(mx, u1, v1);
            const double du = std::max(4.0, std::fabs(u1 - u0)), dv = std::max(4.0, std::fabs(v1 - v0));
            v.ortho.zoom = std::clamp(std::min(w / du, h / dv) * 0.7, OrthoCamera::kMinZoom, OrthoCamera::kMaxZoom);
        }
        v.dirty = true;
    }
}

// ---------------------------------------------------------------- actions

bool Editor::enabled(Act a) const {
    if (actionInfo(a).milestone != 0) return false;
    const bool hasSel = !doc_.sel.empty();
    const bool hasFaces = !doc_.model.mesh.faces.empty();
    switch (a) {
    case Act::Undo: return doc_.canUndo();
    case Act::Redo: return doc_.canRedo();
    case Act::Duplicate:
    case Act::Hide:
    case Act::HideOthers:
    case Act::Delete:
    case Act::Transform:
    case Act::MovePoints:
    case Act::SnapToGrid:
    case Act::Flatten:
    case Act::Grow:
    case Act::Shrink: return hasSel;
    case Act::Mirror: return hasSel && (mode_ == SelectMode::Face || mode_ == SelectMode::Piece);
    case Act::SimilarTexture:
    case Act::SimilarSmoothing:
    case Act::SimilarDirection: return hasSel && (mode_ == SelectMode::Face || mode_ == SelectMode::Piece);
    case Act::EdgeLoop:
    case Act::EdgeRing: return mode_ == SelectMode::Edge && !doc_.sel.edges.empty();
    case Act::SelectAll: return hasFaces;
    case Act::ExportGlb:
    case Act::ExportGltf:
    case Act::ExportObj: return hasFaces;
    case Act::FlyMode: return flying_ || firstView3D() >= 0;
    case Act::MaximiseView: return layout_.maximised >= 0 || hoveredView_ >= 0;
    default: return true;
    }
}

void Editor::run(Act a) {
    if (!enabled(a)) return;
    switch (a) {
    case Act::New: whenSaved([this] { fileNew(); }); break;
    case Act::Open: whenSaved([this] { fileOpen(); }); break;
    case Act::Save: fileSave(false); break;
    case Act::SaveAs: fileSave(true); break;
    case Act::ExportGlb: fileExport("glb"); break;
    case Act::ExportGltf: fileExport("gltf"); break;
    case Act::ExportObj: fileExport("obj"); break;
    case Act::Quit: requestQuit(); break;
    case Act::Undo: if (doc_.undo()) touch(); break;
    case Act::Redo: if (doc_.redo()) touch(); break;
    case Act::Duplicate: duplicateSelected(); break;
    case Act::Delete: deleteSelected(); break;
    case Act::SelectAll: selectAll(); break;
    case Act::SelectNone: selectNone(); break;
    case Act::ModeVertex: setMode(SelectMode::Vertex); break;
    case Act::ModeEdge: setMode(SelectMode::Edge); break;
    case Act::ModeFace: setMode(SelectMode::Face); break;
    case Act::ModePiece: setMode(SelectMode::Piece); break;
    case Act::Grow: growShrink(true); break;
    case Act::Shrink: growShrink(false); break;
    case Act::SimilarTexture: selectSimilar(Similar::Texture); break;
    case Act::SimilarSmoothing: selectSimilar(Similar::SmoothingGroup); break;
    case Act::SimilarDirection: selectSimilar(Similar::Direction); break;
    case Act::EdgeLoop: selectLoop(false); break;
    case Act::EdgeRing: selectLoop(true); break;
    case Act::Hide: hideSelected(false); break;
    case Act::HideOthers: hideSelected(true); break;
    case Act::ShowAll: showAll(); break;
    case Act::CreatePlane: startPrimitive(PrimKind::Plane); break;
    case Act::CreateBox: startPrimitive(PrimKind::Box); break;
    case Act::CreateCylinder: startPrimitive(PrimKind::Cylinder); break;
    case Act::CreateCone: startPrimitive(PrimKind::Cone); break;
    case Act::CreateTube: startPrimitive(PrimKind::Tube); break;
    case Act::CreateTorus: startPrimitive(PrimKind::Torus); break;
    case Act::CreateSphere: startPrimitive(PrimKind::Sphere); break;
    case Act::CreateGeosphere: startPrimitive(PrimKind::Geosphere); break;
    case Act::CreateRock: startPrimitive(PrimKind::Rock); break;
    case Act::ToolSelect: setTool(Tool::Select); break;
    case Act::ToolVertex: setTool(Tool::Vertex); break;
    case Act::ToolPrimitives: setTool(Tool::Primitives); break;
    case Act::Transform: transformOpen_ = true; break;
    case Act::MovePoints: movePointsOpen_ = true; break;
    case Act::SnapToGrid: snapSelectedToGrid(); break;
    case Act::Mirror: mirrorOpen_ = true; break;
    case Act::Flatten: flattenOpen_ = true; break;
    case Act::SetPivot: pivotTyped_ = doc_.model.pivot; pivotOpen_ = true; break;
    case Act::GridSmaller: grid_.halve(); markViewsDirty(); break;
    case Act::GridLarger: grid_.doubleUp(); markViewsDirty(); break;
    case Act::GridSet: gridDialogValue_ = grid_.size; gridDialogOpen_ = true; break;
    case Act::ToggleGrid: grid_.visible = !grid_.visible; markViewsDirty(); break;
    case Act::ToggleSnap: grid_.snap = !grid_.snap; break;
    case Act::FlyMode: {
        if (flying_) { setFlying(false, -1); break; }
        const int v = (hoveredView_ >= 0 && views_[hoveredView_].is3D()) ? hoveredView_ : firstView3D();
        if (v >= 0) setFlying(true, v);
        break;
    }
    case Act::OrbitOrigin:
        orbitSelection_ = false;
        for (View& v : views_)
            if (v.is3D()) {
                v.cam3d.distance = std::max(1.0, length(v.cam3d.eye() - Vec3{}));
                v.cam3d.target = {};
                v.dirty = true;
            }
        break;
    case Act::OrbitSelection: {
        orbitSelection_ = true;
        Vec3 mn, mx;
        if (selectionBounds(mn, mx)) {
            const Vec3 c = (mn + mx) / 2;
            for (View& v : views_)
                if (v.is3D()) {
                    // Keep the camera where it is, now turning around the selection.
                    const Vec3 eye = v.cam3d.eye();
                    v.cam3d.distance = std::max(1.0, length(eye - c));
                    v.cam3d.target = c;
                    const Vec3 f = normalize(c - eye);
                    v.cam3d.pitch = std::clamp(degrees(std::asin(std::clamp(-f.z, -1.0, 1.0))), -89.0, 89.0);
                    v.cam3d.yaw = degrees(std::atan2(-f.x, f.y));
                    v.dirty = true;
                }
        }
        break;
    }
    case Act::FrameSelection: frameSelection(); break;
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
    case Act::RefImages: refsOpen_ = !refsOpen_; break;
    case Act::ScaleFigure: showFigure_ = !showFigure_; touch(); break;
    case Act::WireOverlay: wireOverlay_ = !wireOverlay_; touch(); break;
    case Act::ShowRefs: showRefs_ = !showRefs_; touch(); break;
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
        if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) continue;
        const bool plain = !c.shift && !c.cmd;
        if (c.key == GLFW_KEY_ESCAPE && plain) {
            // Esc stops the most immediate thing first.
            if (flying_) { setFlying(false, -1); continue; }
            if (drag_ != DragKind::None) { cancelDrag(); continue; }
            if (primActive_) { cancelPrimitive(); continue; }
        }
        if (c.key == GLFW_KEY_ENTER && plain) {
            if (primActive_) createPrimitive();
            continue;
        }
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

void Editor::requestQuit() {
    whenSaved([this] { quit_ = true; });
}

void Editor::updateTitle() {
    const std::string t = "Mazzuola - " + displayName() + (doc_.dirty ? " *" : "");
    if (t != lastTitle_) {
        glfwSetWindowTitle(window_, t.c_str());
        lastTitle_ = t;
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

    controlsWindow();
    aboutWindow();
    gridSizeDialog();
    penTestWindow();
    transformDialog();
    movePointsDialog();
    mirrorDialog();
    flattenDialog();
    pivotDialog();
    refsWindow();
    unsavedPrompt();
    pathDialog();
    messageBox();
    updateTitle();

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
    return flying_ || draggingView_ >= 0 || drag_ != DragKind::None || scriptBusy || !screenshotRequest_.empty() ||
           !opt_.screenshot.empty();
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
        ImGui::DockBuilderSplitNode(id, ImGuiDir_Right, 0.19f, &right, &left);
        ImGuiID views, texture;
        ImGui::DockBuilderSplitNode(left, ImGuiDir_Right, 0.27f, &texture, &views);
        ImGuiID tools, rest, props, bottom;
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.36f, &tools, &rest);
        ImGui::DockBuilderSplitNode(rest, ImGuiDir_Up, 0.68f, &props, &bottom);
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
    std::string label = info.label;
    if (a == Act::Undo && doc_.canUndo()) label = "Undo " + doc_.undoName();
    if (a == Act::Redo && doc_.canRedo()) label = "Redo " + doc_.redoName();
    label += std::string("##") + info.id;
    if (ImGui::MenuItem(label.c_str(), keys.empty() ? nullptr : keys.c_str(), selected, enabled(a))) run(a);
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
        if (ImGui::BeginMenu("Export")) {
            menuItem(Act::ExportGlb);
            menuItem(Act::ExportGltf);
            menuItem(Act::ExportObj);
            ImGui::EndMenu();
        }
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
        menuItem(Act::ModeVertex, mode_ == SelectMode::Vertex);
        menuItem(Act::ModeEdge, mode_ == SelectMode::Edge);
        menuItem(Act::ModeFace, mode_ == SelectMode::Face);
        menuItem(Act::ModePiece, mode_ == SelectMode::Piece);
        ImGui::Separator();
        menuItem(Act::Grow);
        menuItem(Act::Shrink);
        if (ImGui::BeginMenu("Select similar")) {
            menuItem(Act::SimilarTexture);
            menuItem(Act::SimilarSmoothing);
            menuItem(Act::SimilarDirection);
            ImGui::EndMenu();
        }
        menuItem(Act::EdgeLoop);
        menuItem(Act::EdgeRing);
        ImGui::Separator();
        menuItem(Act::Hide);
        menuItem(Act::HideOthers);
        menuItem(Act::ShowAll);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Tools")) {
        for (Act a : {Act::ToolSelect, Act::ToolVertex, Act::ToolPrimitives, Act::ToolDraw, Act::ToolPushPull,
                      Act::ToolExtrude, Act::ToolCut, Act::ToolLatheLoft, Act::ToolBrush, Act::ToolMapping,
                      Act::ToolPaint, Act::ToolColour}) {
            const bool on = (a == Act::ToolSelect && tool_ == Tool::Select) || (a == Act::ToolVertex && tool_ == Tool::Vertex) ||
                            (a == Act::ToolPrimitives && tool_ == Tool::Primitives);
            menuItem(a, on);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Create")) {
        if (ImGui::BeginMenu("Primitives")) {
            for (Act a : {Act::CreatePlane, Act::CreateBox, Act::CreateCylinder, Act::CreateCone, Act::CreateTube,
                          Act::CreateTorus, Act::CreateSphere, Act::CreateGeosphere})
                menuItem(a);
            ImGui::EndMenu();
        }
        menuItem(Act::CreateRock);
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
        menuItem(Act::Mirror);
        menuItem(Act::Flatten);
        menuItem(Act::SetPivot);
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
        menuItem(Act::RefImages, refsOpen_);
        menuItem(Act::ShowRefs, showRefs_);
        menuItem(Act::ScaleFigure, showFigure_);
        menuItem(Act::WireOverlay, wireOverlay_);
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
            ImGui::Text("%s tool", toolName(tool_));
            ImGui::TextDisabled("|");
            const Selection& s = doc_.sel;
            size_t n = 0;
            switch (mode_) {
            case SelectMode::Vertex: n = s.verts.size(); break;
            case SelectMode::Edge: n = s.edges.size(); break;
            default: n = s.faces.size(); break;
            }
            if (n) ImGui::Text("%zu %s selected", n, mode_ == SelectMode::Piece ? "faces (pieces)" : selectModeName(mode_));
            else ImGui::Text("%s, nothing selected", selectModeName(mode_));
            ImGui::TextDisabled("|");
            if (flying_) {
                ImGui::TextUnformatted("Flying: WASD, Q/E down/up, Shift fast, Z or Esc to stop");
            } else if (primActive_) {
                ImGui::TextUnformatted("Return creates, Esc cancels");
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
            // Counter: totals, then selected and hidden when there are any.
            const Mesh& m = doc_.model.mesh;
            int hiddenTris = 0;
            for (const Face& fc : m.faces)
                if (fc.hidden) hiddenTris += fc.n == 4 ? 2 : 1;
            char count[160];
            int len = std::snprintf(count, sizeof count, "%zu vertices, %d triangles", m.verts.size(), m.triangleCount());
            if (!s.faces.empty() && (mode_ == SelectMode::Face || mode_ == SelectMode::Piece)) {
                int st = 0;
                for (int fi : s.faces)
                    if (fi >= 0 && fi < (int)m.faces.size()) st += m.faces[fi].n == 4 ? 2 : 1;
                len += std::snprintf(count + len, sizeof count - len, " (%d selected)", st);
            }
            if (hiddenTris) std::snprintf(count + len, sizeof count - len, ", %d hidden", hiddenTris);
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
