#include "app/editor.hpp"
#include "core/select_ops.hpp"
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
constexpr const char* kWinToolbar = "Tool bar";
constexpr const char* kWinProps = "Properties";
constexpr const char* kWinGroups = "Groups";
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
    // 3D | Top over Front | Right, as in Scalpellino (user, 2026-11-20).
    views_[0].setDir(ViewDir::Persp);
    views_[1].setDir(ViewDir::Top);
    views_[2].setDir(ViewDir::Front);
    views_[3].setDir(ViewDir::Right);
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
    case Act::Delete:
    case Act::Transform:
    case Act::MovePoints:
    case Act::Replicate:
    case Act::Flatten:
    case Act::Subdivide:
    case Act::DivideEdges:
    case Act::Poke:
    case Act::Connect:
    case Act::MergeCentre:
    case Act::Collapse:
    case Act::Weld:
    case Act::TearOpen:
    case Act::Fill:
    case Act::Detach:
    case Act::FlipFaces:
    case Act::FlipDiagonal:
    case Act::Triangulate:
    case Act::JoinQuads:
    case Act::TwoSidedOn:
    case Act::TwoSidedOff:
    case Act::Duplicate:
    case Act::MirrorX:
    case Act::MirrorY:
    case Act::MirrorZ:
    case Act::MirrorCloneX:
    case Act::MirrorCloneY:
    case Act::MirrorCloneZ:
    case Act::SnapToGrid: return !doc_.sel.empty();
    case Act::SelectGrow:
    case Act::SelectShrink: return !doc_.sel.empty() && doc_.sel.mode != SelectMode::Piece;
    case Act::SelectLoop:
    case Act::SelectRing: return !doc_.sel.empty() && doc_.sel.mode != SelectMode::Piece;
    case Act::Dissolve: return !doc_.sel.empty() && doc_.sel.mode != SelectMode::Piece;
    case Act::MergeFirst: return doc_.sel.mode == SelectMode::Vert && doc_.sel.verts.size() >= 2 && doc_.firstVert() >= 0;
    case Act::ExportGlb:
    case Act::ExportGltf:
    case Act::ExportObj:
    case Act::ExportAgain: return !doc_.mesh.faces.empty();
    case Act::Undo: return doc_.canUndo();
    case Act::Redo: return doc_.canRedo();
    default: return true;
    }
}

void Editor::run(Act a) {
    if (!enabled(a)) return;
    // Selection commands while a tool shows a preview: the preview goes first (tools
    // that follow the selection bring it back for the new one).
    if ((a == Act::SelectAll || a == Act::SelectNone || a == Act::SelectGrow || a == Act::SelectShrink ||
         a == Act::SelectLoop || a == Act::SelectRing) &&
        doc_.liveActive() && op_ == Op::None)
        cancelTool();
    switch (a) {
    case Act::Quit: requestQuit(); break;
    case Act::New: fileNew(); break;
    case Act::Open: fileOpen(); break;
    case Act::Save: fileSave(false); break;
    case Act::SaveAs: fileSave(true); break;
    case Act::ExportGlb: fileExport(ExportFormat::Glb); break;
    case Act::ExportGltf: fileExport(ExportFormat::Gltf); break;
    case Act::ExportObj: fileExport(ExportFormat::Obj); break;
    case Act::ExportAgain:
        if (exportPath_.empty()) fileExport(exportFormat_);
        else exportPath(exportPath_, exportFormat_);
        break;
    case Act::Undo: cancelTool(); if (doc_.undo()) touch(); break;
    case Act::Redo: cancelTool(); if (doc_.redo()) touch(); break;
    case Act::Delete:
        // Pieces go; points, edges and faces dissolve (the model stays closed).
        if (doc_.sel.mode == SelectMode::Piece) cleanUp(Act::TearOpen);
        else cleanUp(Act::Dissolve);
        break;
    case Act::MergeCentre:
    case Act::MergeFirst:
    case Act::Collapse:
    case Act::Dissolve:
    case Act::TearOpen:
    case Act::Fill:
    case Act::Detach:
    case Act::FlipFaces:
    case Act::FlipDiagonal:
    case Act::Triangulate:
    case Act::JoinQuads:
    case Act::TwoSidedOn:
    case Act::TwoSidedOff: cleanUp(a); break;
    case Act::Weld: openOp(Op::Weld); break;
    case Act::Transform:
    case Act::MovePoints: openOp(Op::Transform); break;
    case Act::Replicate: openOp(Op::Replicate); break;
    case Act::Flatten: openOp(Op::Flatten); break;
    case Act::Subdivide: openOp(Op::Subdivide); break;
    case Act::DivideEdges: openOp(Op::DivideEdges); break;
    case Act::Poke: openOp(Op::Poke); break;
    case Act::Connect: {
        if (doc_.sel.mode != SelectMode::Vert || doc_.sel.verts.size() != 2) {
            showMessage("Connect two points", "Select exactly two points (point mode) on the same face.");
            break;
        }
        const int a = *doc_.sel.verts.begin(), b = *doc_.sel.verts.rbegin();
        std::string why;
        if (!doc_.apply("Connect", [&](MeshEdit& e, Selection&) { return connectPoints(e, a, b, &why); }))
            showMessage("Connect two points", "Can't: " + why + ".");
        touch();
        break;
    }
    case Act::ToolCut: setTool(&cutTool_); break;
    case Act::Duplicate:
    case Act::MirrorX:
    case Act::MirrorY:
    case Act::MirrorZ:
    case Act::MirrorCloneX:
    case Act::MirrorCloneY:
    case Act::MirrorCloneZ:
    case Act::SnapToGrid: {
        cancelTool();
        if (doc_.sel.empty()) break;
        const Selection was = doc_.sel;
        const char* name = a == Act::Duplicate ? "Duplicate" : a == Act::SnapToGrid ? "Snap to grid"
                           : (a == Act::MirrorCloneX || a == Act::MirrorCloneY || a == Act::MirrorCloneZ) ? "Mirror clone"
                                                                                                          : "Mirror";
        doc_.apply(name, [&](MeshEdit& e, Selection& sel) {
            std::set<int> made;
            switch (a) {
            case Act::Duplicate: made = duplicate(e, was); break;
            case Act::MirrorX: mirrorInPlace(e, was, 0); break;
            case Act::MirrorY: mirrorInPlace(e, was, 1); break;
            case Act::MirrorZ: mirrorInPlace(e, was, 2); break;
            case Act::MirrorCloneX: made = mirrorClone(e, was, 0); break;
            case Act::MirrorCloneY: made = mirrorClone(e, was, 1); break;
            case Act::MirrorCloneZ: made = mirrorClone(e, was, 2); break;
            default:
                for (int v : selectedVerts(e.mesh(), was)) {
                    const Vec3 q = e.mesh().verts[v];
                    e.setVert(v, {grid_.snapValue(q.x), grid_.snapValue(q.y), grid_.snapValue(q.z)});
                }
                break;
            }
            if (!made.empty()) {
                // The copy ends up selected, in the same mode.
                Selection copy;
                copy.mode = SelectMode::Face;
                copy.faces = made;
                sel = convertSelection(e.mesh(), copy, was.mode);
            }
            return true;
        });
        touch();
        break;
    }
    case Act::SelectAll: {
        Selection s;
        s.mode = SelectMode::Piece;
        for (int i = 0; i < (int)doc_.mesh.faces.size(); ++i) s.faces.insert(i);
        doc_.select(s);
        touch();
        break;
    }
    case Act::SelectNone: {
        Selection s;
        s.mode = doc_.sel.mode;
        doc_.select(s);
        touch();
        break;
    }
    // Points, edges and faces arrive in v0.1.2 (they convert the selection then).
    case Act::ModeVertex:
    case Act::ModeEdge:
    case Act::ModeFace:
    case Act::ModePiece: {
        // Switching mode converts the selection (ARCHITECTURE §6).
        const SelectMode to = a == Act::ModeVertex ? SelectMode::Vert
                              : a == Act::ModeEdge ? SelectMode::Edge
                              : a == Act::ModeFace ? SelectMode::Face
                                                   : SelectMode::Piece;
        cancelTool();
        // The Vertex tool works on points only: edges, faces or pieces go to Select.
        if (tool_ == &vertexTool_ && to != SelectMode::Vert) setTool(&selectTool_);
        doc_.select(convertSelection(doc_.mesh, doc_.sel, to));
        touch();
        break;
    }
    case Act::SelectGrow:
        doc_.select(growSelection(doc_.mesh, doc_.sel));
        touch();
        break;
    case Act::SelectShrink:
        doc_.select(shrinkSelection(doc_.mesh, doc_.sel));
        touch();
        break;
    case Act::SelectLoop:
    case Act::SelectRing: {
        // The loop / ring of every selected edge (also from points or faces: their edges).
        const Selection e = convertSelection(doc_.mesh, doc_.sel, SelectMode::Edge);
        Selection out;
        out.mode = SelectMode::Edge;
        for (const Edge& edge : e.edges)
            for (const Edge& x : a == Act::SelectLoop ? edgeLoop(doc_.mesh, edge) : edgeRing(doc_.mesh, edge))
                out.edges.insert(x);
        doc_.select(convertSelection(doc_.mesh, out, doc_.sel.mode == SelectMode::Vert ? SelectMode::Vert : SelectMode::Edge));
        touch();
        break;
    }
    case Act::ToolSelect: setTool(&selectTool_); break;
    case Act::ToolPrimitives: setTool(&primTool_); break;
    case Act::ToolPushPull: setTool(&pushPullTool_); break;
    case Act::ToolExtrude: setTool(&extrudeTool_); break;
    case Act::ToolInset: setTool(&insetTool_); break;
    case Act::ToolDraw: setTool(&drawTool_); break;
    case Act::ToolVertex:
        // The Vertex tool works on points.
        setTool(&vertexTool_);
        doc_.select(convertSelection(doc_.mesh, doc_.sel, SelectMode::Vert));
        touch();
        break;
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
    case Act::ScaleFigure: showFigure_ = !showFigure_; markViewsDirty(); break;
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
        if (op_ != Op::None && !c.shift && !c.cmd && (c.key == GLFW_KEY_ENTER || c.key == GLFW_KEY_ESCAPE)) {
            if (c.key == GLFW_KEY_ENTER) applyOp();
            else cancelOp();
            continue;
        }
        {
            ToolEnv env = toolEnv();
            if (tool_->key(env, c.key, c.shift, c.cmd)) continue;
        }
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

    {
        ToolEnv env = toolEnv();
        tool_->update(env);
    }
    refreshModelDrawing();
    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(kWinViews, nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse))
        viewsPanel();
    ImGui::End();
    if (ImGui::Begin(kWinTexture, nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) textureView();
    ImGui::End();
    if (ImGui::Begin(kWinToolbar, nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar)) toolBar();
    ImGui::End();
    if (ImGui::Begin(kWinProps)) propertiesPanel();
    ImGui::End();
    if (ImGui::Begin(kWinGroups)) groupsPanel();
    ImGui::End();
    if (ImGui::Begin(kWinPalette)) palettePanel();
    ImGui::End();

    opWindow();
    refWindow();
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
        // Tool bar | views | texture view | Properties over Groups/Palette.
        const float barW = std::max(0.04f, std::min(0.2f, 120.0f / std::max(1.0f, vp->WorkSize.x)));
        ImGuiID bar, main;
        ImGui::DockBuilderSplitNode(id, ImGuiDir_Left, barW, &bar, &main);
        ImGuiID left, right;
        ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, 0.17f, &right, &left);
        ImGuiID views, texture;
        ImGui::DockBuilderSplitNode(left, ImGuiDir_Right, 0.30f, &texture, &views);
        ImGuiID props, bottom;
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.62f, &props, &bottom);
        ImGui::DockBuilderDockWindow(kWinToolbar, bar);
        ImGui::DockBuilderDockWindow(kWinViews, views);
        ImGui::DockBuilderDockWindow(kWinTexture, texture);
        ImGui::DockBuilderDockWindow(kWinProps, props);
        ImGui::DockBuilderDockWindow(kWinGroups, bottom);
        ImGui::DockBuilderDockWindow(kWinPalette, bottom);
        if (ImGuiDockNode* n = ImGui::DockBuilderGetNode(views)) n->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
        if (ImGuiDockNode* n = ImGui::DockBuilderGetNode(bar)) n->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
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
        if (ImGui::BeginMenu("Export", !doc_.mesh.faces.empty())) {
            menuItem(Act::ExportGlb);
            menuItem(Act::ExportGltf);
            menuItem(Act::ExportObj);
            ImGui::EndMenu();
        }
        menuItem(Act::ExportAgain);
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
        menuItem(Act::SelectGrow);
        menuItem(Act::SelectShrink);
        menuItem(Act::SelectLoop);
        menuItem(Act::SelectRing);
        plannedItem("Select similar", 1);
        ImGui::Separator();
        menuItem(Act::Hide);
        menuItem(Act::HideOthers);
        menuItem(Act::ShowAll);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Tools")) {
        for (Act a : {Act::ToolSelect, Act::ToolVertex, Act::ToolPrimitives, Act::ToolDraw, Act::ToolPushPull,
                      Act::ToolExtrude, Act::ToolInset, Act::ToolCut, Act::ToolLatheLoft, Act::ToolBrush, Act::ToolMapping,
                      Act::ToolPaint, Act::ToolColour})
            menuItem(a, (a == Act::ToolSelect && tool_ == &selectTool_) || (a == Act::ToolPrimitives && tool_ == &primTool_) ||
                            (a == Act::ToolVertex && tool_ == &vertexTool_) || (a == Act::ToolPushPull && tool_ == &pushPullTool_) ||
                            (a == Act::ToolExtrude && tool_ == &extrudeTool_) || (a == Act::ToolInset && tool_ == &insetTool_) ||
                            (a == Act::ToolDraw && tool_ == &drawTool_) || (a == Act::ToolCut && tool_ == &cutTool_));
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Create")) {
        if (ImGui::BeginMenu("Primitives")) {
            // Each opens the Primitives tool with that shape: then drag in a 2D view.
            for (int k = 0; k < kPrimKindCount; ++k) {
                if ((PrimKind)k == PrimKind::Rock) continue; // its own entry below
                if (ImGui::MenuItem(primKindName((PrimKind)k), nullptr, tool_ == &primTool_ && (int)primTool_.params().kind == k)) {
                    primTool_.setShape((PrimKind)k);
                    setTool(&primTool_);
                    touch();
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Rock", nullptr, tool_ == &primTool_ && primTool_.params().kind == PrimKind::Rock)) {
            primTool_.setShape(PrimKind::Rock);
            setTool(&primTool_);
            touch();
        }
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
        menuItem(Act::Replicate);
        menuItem(Act::MovePoints);
        menuItem(Act::SnapToGrid);
        ImGui::Separator();
        menuItem(Act::MirrorX);
        menuItem(Act::MirrorY);
        menuItem(Act::MirrorZ);
        menuItem(Act::MirrorCloneX);
        menuItem(Act::MirrorCloneY);
        menuItem(Act::MirrorCloneZ);
        menuItem(Act::Flatten);
        ImGui::Separator();
        menuItem(Act::Subdivide);
        menuItem(Act::DivideEdges);
        menuItem(Act::Poke);
        menuItem(Act::Connect);
        ImGui::Separator();
        menuItem(Act::MergeCentre);
        menuItem(Act::MergeFirst);
        menuItem(Act::Collapse);
        menuItem(Act::Weld);
        menuItem(Act::Dissolve);
        menuItem(Act::TearOpen);
        menuItem(Act::Fill);
        menuItem(Act::Detach);
        ImGui::Separator();
        menuItem(Act::FlipFaces);
        menuItem(Act::FlipDiagonal);
        menuItem(Act::Triangulate);
        menuItem(Act::JoinQuads);
        menuItem(Act::TwoSidedOn);
        menuItem(Act::TwoSidedOff);
        ImGui::Separator();
        plannedItem("Bevel, bridge, shell", 2);
        plannedItem("Mirror mode", 2);
        ImGui::Separator();
        plannedItem("Groups, smoothing groups", 3);
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
        menuItem(Act::ScaleFigure, showFigure_);
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
            // Left: tool and what its gestures do (as in Scalpellino). Right: selection,
            // cursor, grid, counts.
            ImGui::TextUnformatted(tool_->name());
            ImGui::TextDisabled("|");
            const char* hintText = flying_ ? "flying: WASD move, Q/E down/up, Shift fast, Z or Esc stops" : tool_->hint();
            // Right after an export, until the model changes: where it went.
            if (!flying_ && !exportNote_.empty() && exportNoteMesh_ == doc_.meshVersion()) hintText = exportNote_.c_str();
            const float hintX = ImGui::GetCursorScreenPos().x;
            const float hintY = ImGui::GetCursorScreenPos().y;
            std::string right;
            char buf[160];
            if (doc_.sel.empty()) std::snprintf(buf, sizeof buf, "%s, nothing selected", selectModeName(doc_.sel.mode));
            else {
                const SelectMode md = doc_.sel.mode;
                const int n = md == SelectMode::Vert ? (int)doc_.sel.verts.size()
                              : md == SelectMode::Edge ? (int)doc_.sel.edges.size()
                                                       : (int)doc_.sel.faces.size();
                std::snprintf(buf, sizeof buf, "%s, %d selected", selectModeName(md), n);
            }
            right += buf;
            if (haveCursorWorld_ && cursorView_ >= 0 && !flying_) {
                const OrthoAxes ax = orthoAxes(views_[cursorView_].dir);
                static const char* names[] = {"x", "y", "z"};
                const int a0 = std::min(ax.u, ax.v), a1 = std::max(ax.u, ax.v);
                std::snprintf(buf, sizeof buf, "   |   %s %.2f  %s %.2f cm", names[a0], cursorWorld_[a0], names[a1], cursorWorld_[a1]);
                right += buf;
            }
            std::snprintf(buf, sizeof buf, "   |   grid %g cm%s%s", grid_.size, grid_.snap ? ", snap" : "", grid_.visible ? "" : ", hidden");
            right += buf;
            int tris = 0;
            for (const Face& fc : doc_.mesh.faces) tris += fc.n - 2;
            std::snprintf(buf, sizeof buf, "   |   %d vertices, %d triangles", (int)doc_.mesh.verts.size(), tris);
            right += buf;
            // The right part always shows in full; the hint gets what's left (with "...").
            const float w = ImGui::CalcTextSize(right.c_str()).x;
            const float x = ImGui::GetWindowWidth() - w - ImGui::GetStyle().WindowPadding.x - 4;
            const float rightScreenX = ImGui::GetWindowPos().x + x;
            const float hintMax = rightScreenX - ImGui::GetStyle().ItemSpacing.x * 2;
            ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), ImVec2(hintX, hintY),
                                      ImVec2(hintMax, hintY + ImGui::GetTextLineHeight()), hintMax, hintText,
                                      nullptr, nullptr);
            ImGui::SameLine();
            ImGui::SetCursorPosX(std::max(x, ImGui::GetCursorPosX()));
            ImGui::TextUnformatted(right.c_str());
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
}

} // namespace mazz
