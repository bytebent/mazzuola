#pragma once
// The app's state and per-frame logic. Real input and scripted input (tests,
// screenshots) go through the same paths: key chords -> actions, mouse -> ImGui IO.
//
// Split over several files:
//   app/editor.cpp       frame, actions, menus, status bar, dock layout
//   app/tools.cpp        select / vertex / primitives tools, handles, picking, previews
//   app/file_ops.cpp     new / open / save / export, unsaved-changes questions
//   app/script.cpp       scripted input (see docs/DEV.md)
//   ui/views_panel.cpp   the four views, their headers, navigation, overlays
//   ui/panels.cpp        side panels, texture view, Controls, About
//   ui/dialogs.cpp       Transform, Move points, Mirror, Flatten, Pivot, Reference images
#include "core/document.hpp"
#include "core/grid.hpp"
#include "core/ops.hpp"
#include "core/pick.hpp"
#include "core/primitives.hpp"
#include "render/mesh_draw.hpp"
#include "render/view_draw.hpp"
#include "ui/actions.hpp"
#include "imgui.h"
#include <deque>
#include <functional>
#include <string>
#include <vector>

struct GLFWwindow;

namespace mazz {

struct EditorOptions {
    std::string scriptPath;  // --script <file>: scripted input, see tests/ui
    std::string screenshot;  // --screenshot <file.png>: one shot after start-up, then quit
    std::string openPath;    // --open <file.mazz>
    bool automated() const { return !scriptPath.empty() || !screenshot.empty(); }
};

const char* selectModeName(SelectMode m);

enum class Tool { Select, Vertex, Primitives };
const char* toolName(Tool t);

// What a drag in a view does.
enum class DragKind { None, Box, Move, Scale, Rotate, CreatePrim, PlacePivot };

// Handles around the selection in 2D views: scale or rotate (a click on the selection
// switches between them, as in Hammer).
enum class HandleMode { Scale, Rotate };

// The 2x2 views area: split position, and which view (if any) fills it.
struct ViewsLayout {
    float splitX = 0.5f, splitY = 0.5f;
    int maximised = -1;
};

// Pivot choices for Transform and Mirror.
enum class PivotChoice { SelectionCentre, ModelPivot, Origin, Typed };

class Editor {
public:
    Editor(GLFWwindow* window, const EditorOptions& opt);
    ~Editor();

    // One frame of UI. Returns true while something needs continuous frames.
    bool frame(double dt);
    bool shouldExit() const { return quit_; }
    int exitCode() const { return scriptFailures_ > 0 ? 1 : 0; }
    // The window's close button / Cmd+Q: asks about unsaved changes first.
    void requestQuit();

    // A physical key press (from the GLFW callback or a script).
    void keyPressed(int glfwKey, bool shift, bool cmd);

    std::string takeScreenshotRequest();
    double viewScale() const;

    void run(Act a);
    bool enabled(Act a) const;

    // The paint window (a second app window; main.cpp opens and closes it).
    bool paintWindowWanted() const { return paintWanted_; }
    void paintWindowClosed() { paintWanted_ = false; }
    void setPaintWindowOpen(bool open) { paintOpen_ = open; }
    std::string takePaintShot() { std::string s; s.swap(paintShot_); return s; }
    const Grid& grid() const { return grid_; }
    // The app came back to the front: images may have changed on disk.
    void windowFocused();
    // What the views show (the paint window draws it too, without reference images).
    Scene scene(bool withRefs);

private:
    // ---- frame parts (editor.cpp)
    void dockspace();
    void menuBar();
    void statusBar();
    void menuItem(Act a, bool selected = false);
    void plannedItem(const char* label, int milestone);
    void handleKeys();
    void updateFly(double dt);
    void setFlying(bool on, int view);
    int firstView3D() const;
    void markViewsDirty();
    void allViewsToOrigin();
    void frameSelection();
    void updateTitle();

    // ---- model changes and tools (tools.cpp)
    void touch();                        // something the views show changed
    bool edit(const std::string& what, const std::function<bool(Model&, Selection&)>& fn);
    void setMode(SelectMode m);
    void setTool(Tool t);
    std::set<int> movingVerts() const;   // points the current selection moves
    bool selectionBounds(Vec3& mn, Vec3& mx) const;
    Vec3 pivotPoint(PivotChoice c, const Vec3& typed) const;
    void selectAll();
    void selectNone();
    void deleteSelected();
    void duplicateSelected();
    void hideSelected(bool others);
    void showAll();
    void snapSelectedToGrid();
    void growShrink(bool grow);
    void selectSimilar(Similar how);
    void selectLoop(bool ring);
    void updatePrimPreview();
    void createPrimitive();
    void cancelPrimitive();
    void newSeed();
    void startPrimitive(PrimKind k);
    Projector projector(int view) const;
    void viewMouse(int view, double mx, double my, bool hovered, bool active);
    void clickSelect(int view, double mx, double my, bool add);
    bool handleAt(int view, double mx, double my, int& handle) const;
    bool insideSelection(int view, double mx, double my) const;
    void applyDrag(int view, double mx, double my, bool final);
    void cancelDrag();
    void drawOverlay(int view, ImDrawList* dl, ImVec2 origin, ImVec2 size);
    Vec3 figurePosition() const;

    // ---- files (file_ops.cpp)
    void fileNew();
    void fileOpen();
    void fileSave(bool saveAs);
    void fileExport(const std::string& ext);
    void loadRefImage();
    bool openPath(const std::string& path);
    bool savePath(const std::string& path);
    bool exportPath(const std::string& path);
    // Runs `then` now if there are no unsaved changes, otherwise after the user chose
    // Save or Don't save (nothing on Cancel).
    void whenSaved(std::function<void()> then);
    void unsavedPrompt();
    // Typed-path dialog, used where native file dialogs aren't available.
    void askPath(const std::string& title, bool save, const std::string& start, std::function<void(const std::string&)> done);
    void pathDialog();
    void messageBox();
    void showMessage(const std::string& title, const std::string& text);
    std::string modelDir() const;
    std::string displayName() const;
    // Reference image paths: stored relative to the model file when possible.
    void rebaseRefs(Model& m, const std::string& oldDir, const std::string& newDir) const;

    // ---- panels and dialogs (ui/*.cpp)
    void viewsPanel();
    void viewCell(int i, ImVec2 pos, ImVec2 size);
    void viewInput(int i, ImVec2 imgPos, ImVec2 imgSize);
    void textureView();
    void toolsPanel();
    void propertiesPanel();
    void primitivePanel();
    void partsPanel();
    void palettePanel();
    void controlsWindow();
    void aboutWindow();
    void gridSizeDialog();
    void penTestWindow();
    void transformDialog();
    void movePointsDialog();
    void mirrorDialog();
    void flattenDialog();
    void pivotDialog();
    void refsWindow();
    void pivotCombo(const char* id, PivotChoice& c, Vec3& typed);
    // Dialog actions (also used by scripts).
    void applyTransform();
    void applyMovePoints();
    void applyMirror();
    void applyFlatten();

    // ---- scripting (app/script.cpp)
    void loadScript(const std::string& path);
    void stepScript();
    void scriptFail(const std::string& msg);

    GLFWwindow* window_;
    EditorOptions opt_;
    bool quit_ = false;

    Document doc_;
    uint64_t sceneVersion_ = 1;
    Grid grid_;
    View views_[4];
    ViewsLayout layout_;
    ViewDrawer drawer_;
    TextureCache textures_;
    SelectMode mode_ = SelectMode::Face;
    Tool tool_ = Tool::Select;
    HandleMode handleMode_ = HandleMode::Scale;
    bool orbitSelection_ = false;
    bool showFigure_ = false;
    bool wireOverlay_ = true;
    bool showRefs_ = true;

    // primitives tool
    PrimParams prim_;
    bool primActive_ = false;      // a preview exists (drawn or typed), waiting for Return
    Mesh primPreview_;

    // dragging in a view
    DragKind drag_ = DragKind::None;
    int dragView_ = -1;
    int dragHandle_ = -1;          // 0..7 scale handles (corners and sides), 8 = rotate
    double dragX0_ = 0, dragY0_ = 0, dragX_ = 0, dragY_ = 0;
    bool dragMoved_ = false;
    bool dragAdd_ = false;
    bool clickOnSelected_ = false;
    Snapshot dragBase_;
    Vec3 dragMin_, dragMax_;       // selection bounds when the drag started

    // per-view geometry (logical px) and hover
    ImVec2 viewImgPos_[4]{}, viewImgSize_[4]{};
    int hoveredView_ = -1;
    int draggingView_ = -1;
    bool haveCursorWorld_ = false;
    Vec3 cursorWorld_;
    int cursorView_ = -1;

    bool flying_ = false;
    int flyView_ = -1;
    bool flySkipLook_ = false;

    bool resetLayout_ = true;
    bool controlsOpen_ = false, aboutOpen_ = false, gridDialogOpen_ = false, penTestOpen_ = false;
    bool transformOpen_ = false, movePointsOpen_ = false, mirrorOpen_ = false, flattenOpen_ = false;
    bool pivotOpen_ = false, refsOpen_ = false;
    double gridDialogValue_ = 8;
    bool paintWanted_ = false, paintOpen_ = false;
    std::string paintShot_;

    // dialog state
    struct TransformState {
        Vec3 move, rotate, scale{100, 100, 100};
        double alongNormal = 0;
        int copies = 0;
        bool offsetIsTotal = false, scaleOffset = false;
        PivotChoice pivot = PivotChoice::SelectionCentre;
        Vec3 typedPivot;
    } tf_;
    Vec3 movePoints_;
    bool movePointsAbsolute_ = false;
    struct MirrorState {
        int axis = 0;
        bool copy = false;
        PivotChoice pivot = PivotChoice::ModelPivot;
        Vec3 typedPivot;
    } mirror_;
    struct FlattenState {
        int axis = 2;
        int where = 0; // 0 lowest, 1 middle, 2 highest, 3 typed
        double typed = 0;
    } flatten_;
    Vec3 pivotTyped_;
    bool pivotPlacing_ = false;
    Snapshot refBase_;            // reference image field being edited   // next click in a 2D view places the pivot
    int refDir_ = (int)ViewDir::Front;

    // files and questions
    std::function<void()> afterSave_;
    bool unsavedOpen_ = false;
    bool pathDialogOpen_ = false;
    bool pathDialogSave_ = false;
    std::string pathDialogTitle_;
    char pathBuf_[1024] = {};
    std::function<void(const std::string&)> pathDialogDone_;
    bool messageOpen_ = false;
    std::string messageTitle_, messageText_;
    std::string lastTitle_;

    std::deque<Chord> keyQueue_;
    std::string screenshotRequest_;

    // scripting
    std::deque<std::string> script_;
    int scriptLine_ = 0;
    int scriptWait_ = 0;
    int scriptFailures_ = 0;
    bool scriptMode_ = false;
    bool scriptDone_ = false;
    int startupFrames_ = 0;
};

} // namespace mazz
