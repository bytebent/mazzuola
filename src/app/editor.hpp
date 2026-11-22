#pragma once
// The app's state and per-frame logic. Real input and scripted input (tests,
// screenshots) go through the same paths: key chords -> actions, mouse -> ImGui IO.
#include "core/bevel_ops.hpp"
#include "core/document.hpp"
#include "core/grid.hpp"
#include "core/ops.hpp"
#include "io/export.hpp"
#include "render/view_draw.hpp"
#include "tools/cut_tool.hpp"
#include "tools/draw_tool.hpp"
#include "tools/extrude_tool.hpp"
#include "tools/inset_tool.hpp"
#include "tools/prim_tool.hpp"
#include "tools/pushpull_tool.hpp"
#include "tools/select_tool.hpp"
#include "tools/vertex_tool.hpp"
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
    bool automated() const { return !scriptPath.empty() || !screenshot.empty(); }
};

const char* selectModeName(SelectMode m); // "vertices", "edges", "faces", "pieces"

// The 2x2 views area: split position, and which view (if any) fills it.
struct ViewsLayout {
    float splitX = 0.5f, splitY = 0.5f;
    int maximised = -1;
};

class Editor {
public:
    enum class Op { None, Transform, Replicate, Flatten, Subdivide, DivideEdges, Poke, Weld, Bridge, Shell }; // operation windows
    Editor(GLFWwindow* window, const EditorOptions& opt);
    ~Editor();

    // One frame of UI. Returns true while something needs continuous frames
    // (flying, dragging a view, a script running).
    bool frame(double dt);
    bool shouldExit() const { return quit_; }
    int exitCode() const { return scriptFailures_ > 0 ? 1 : 0; }
    // Quit, after asking about unsaved changes (window close button, Cmd+Q).
    void requestQuit();

    // A physical key press (from the GLFW callback or a script).
    void keyPressed(int glfwKey, bool shift, bool cmd);

    // Screenshot wanted after this frame is drawn (empty = none). Clears the request.
    std::string takeScreenshotRequest();
    // Pixels per logical pixel for the views (MAZZ_VIEW_SCALE overrides, for tests).
    double viewScale() const;

    void run(Act a);
    bool enabled(Act a) const;

    // The paint window (a second app window; main.cpp opens and closes it).
    bool paintWindowWanted() const { return paintWanted_; }
    void paintWindowClosed() { paintWanted_ = false; }
    void setPaintWindowOpen(bool open) { paintOpen_ = open; }
    std::string takePaintShot() { std::string s; s.swap(paintShot_); return s; }
    const Grid& grid() const { return grid_; }

private:
    // ---- per-frame parts
    void dockspace();
    void menuBar();
    void statusBar();
    void viewsPanel();           // ui/views_panel.cpp
    void viewCell(int i, ImVec2 pos, ImVec2 size); // ui/views_panel.cpp
    void viewInput(int i, ImVec2 imgPos, ImVec2 imgSize); // ui/views_panel.cpp
    void textureView();          // ui/panels.cpp
    void toolBar();              // ui/panels.cpp (left edge)
    void propertiesPanel();      // ui/panels.cpp
    void groupsPanel();          // ui/panels.cpp
    void palettePanel();         // ui/panels.cpp
    void controlsWindow();       // ui/panels.cpp
    void aboutWindow();          // ui/panels.cpp
    void gridSizeDialog();       // ui/panels.cpp
    void penTestWindow();        // ui/panels.cpp
    void menuItem(Act a, bool selected = false);
    void plannedItem(const char* label, int milestone);
    void handleKeys();
    void updateFly(double dt);
    void setFlying(bool on, int view);
    int firstView3D() const;
    void markViewsDirty();
    void allViewsToOrigin();

    // ---- files (app/file_ops.cpp)
    void fileNew();
    void fileOpen();
    void fileSave(bool saveAs);
    bool openPath(const std::string& path);
    bool savePath(const std::string& path);
    void fileExport(ExportFormat f);                        // asks where, then exports
    bool exportPath(const std::string& path, ExportFormat f); // exports there (no questions)
    void whenSaved(std::function<void()> then); // asks "save changes?" first if needed
    void askPath(const std::string& title, bool saving, const std::string& start,
                 std::function<void(const std::string&)> done);
    void showMessage(const std::string& title, const std::string& text);
    void fileDialogs(); // the pop-ups above, once per frame
    std::string displayName() const;
    void updateTitle();

    // The model changed (geometry or selection): views redraw.
    void touch();

    // ---- operation windows (ui/op_windows.cpp): stay open, live preview, Return / Esc
    void openOp(Op op);
    void applyOp();
    void cancelOp();
    void refreshOpPreview();
    void opWindow();

    // ---- reference images and the scale figure (app/ref_images.cpp)
    struct RefImage {
        unsigned tex = 0;       // GPU picture, 0 = none
        std::string name;       // file name, for the window
        RefPlacement place;
    };
    static int refSlot(ViewDir d) { return d == ViewDir::Persp ? -1 : (int)d; } // one per direction
    RefImage* refFor(int view);                     // the image a 2D view shows (nullptr: none / 3D)
    bool refLoad(int view, const std::string& path); // replaces that direction's image, fills the view
    void refRemove(int slot);
    void refChooseFile(int view);                   // file dialog, then refLoad
    void refWindow();                               // the image's settings while refEdit_ >= 0
    bool refInput(int view, ImVec2 imgPos, ImVec2 imgSize); // handles; true if it took the mouse
    void refOverlay(int view, ImVec2 imgPos, ImVec2 imgSize, ImDrawList* dl);
    ViewExtras viewExtras(int view);

    // ---- tools
    ToolEnv toolEnv();
    void setTool(Tool* t);
    void cancelTool();           // before undo, redo, new, open
    ViewGeom geom(int view) const;
    Pointer pointer(int view) const;

    // ---- scripting (app/script.cpp)
    void loadScript(const std::string& path);
    void stepScript();
    void scriptFail(const std::string& msg);

    GLFWwindow* window_;
    EditorOptions opt_;
    bool quit_ = false;

    Grid grid_;
    View views_[4];
    ViewsLayout layout_;
    ViewDrawer drawer_;
    Document doc_;
    std::string path_;  // the open model's file ("" = never saved)
    double texel_ = 0.5; // texture pixels per cm, kept from the file (milestone 5)
    // The last export of this model (Export again, Cmd+Shift+E); forgotten on New / Open.
    std::string exportPath_;
    ExportFormat exportFormat_ = ExportFormat::Glb;
    std::string exportNote_;    // "Exported to lamp.glb" in the status bar...
    long exportNoteMesh_ = -1;  // ...until the model changes
    bool showFigure_ = false;   // View -> Scale figure (not saved)
    Vec3 figureAt_;             // where it stands (follows the model's box)
    long figureMesh_ = -1;      // model version figureAt_ was worked out for
    RefImage refs_[6];          // reference images by view direction (not saved)
    int refEdit_ = -1;          // direction whose settings window is open, -1 = none
    int refDragView_ = -1;      // view where an image move / resize is going on
    int refDragCorner_ = -1;    // -1 = moving, else the corner held still while resizing
    double refGrabU_ = 0, refGrabV_ = 0;   // where the pointer went down (plane coords)
    double refStartU_ = 0, refStartV_ = 0; // the image's centre then
    SelectTool selectTool_;
    VertexTool vertexTool_;
    PrimTool primTool_;
    PushPullTool pushPullTool_;
    ExtrudeTool extrudeTool_;
    InsetTool insetTool_;
    DrawTool drawTool_;
    CutTool cutTool_;
    Tool* tool_ = &selectTool_;
    LastBox lastBox_;
    // What the views last drew (compared with the document's counters each frame).
    long drawnMesh_ = -1, drawnSel_ = -1;
    DrawData drawData_;
    void refreshModelDrawing();
    int toolView_ = -1; // view where the left button went down (-1 = none)
    bool orbitSelection_ = false; // 3D orbit target: selection (falls back to origin) or origin

    // Per-view screen rects of the image area (logical px), for scripts and the cursor readout.
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
    bool paintWanted_ = false, paintOpen_ = false;
    std::string paintShot_;
    double gridDialogValue_ = 8;

    // operation window
    Op op_ = Op::None;
    Selection opSel_;           // the selection when the window opened
    bool opWasClosed_ = false;  //   and whether the model was closed then
    TransformParams opTransform_;
    ReplicateParams opReplicate_;
    int subdivN_ = 2, subdivM_ = 2; // Subdivide...
    int divideN_ = 2;               // Divide edges...
    double pokeHeight_ = 0;         // Poke...
    double weldDist_ = 0.1;         // Weld by distance... (cm)
    int weldCount_ = 0;             //   points it would remove
    BridgeParams bridge_;           // Bridge...
    ShellParams shell_;             // Shell...
    void cleanUp(Act a);            // Mesh menu clean-up commands (clean_ops)
    std::string opWhy_;             // the operation can't be done as set (shown in its window)
    int flattenAxis_ = 2;
    FlattenTo flattenTo_ = FlattenTo::Average;

    // file pop-ups
    std::function<void()> afterSave_;  // what to do once the unsaved question is settled
    bool unsavedOpen_ = false;
    bool pathOpen_ = false, pathSaving_ = false;
    std::string pathTitle_;
    char pathBuf_[1024] = {};
    std::function<void(const std::string&)> pathDone_;
    bool messageOpen_ = false;
    std::string messageTitle_, messageText_;
    std::string title_;

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
