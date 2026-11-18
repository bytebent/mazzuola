#pragma once
// The app's state and per-frame logic. Real input and scripted input (tests,
// screenshots) go through the same paths: key chords -> actions, mouse -> ImGui IO.
#include "core/grid.hpp"
#include "render/view_draw.hpp"
#include "ui/actions.hpp"
#include "imgui.h"
#include <deque>
#include <string>
#include <vector>

struct GLFWwindow;

namespace mazz {

struct EditorOptions {
    std::string scriptPath;  // --script <file>: scripted input, see tests/ui
    std::string screenshot;  // --screenshot <file.png>: one shot after start-up, then quit
    bool automated() const { return !scriptPath.empty() || !screenshot.empty(); }
};

enum class SelectMode { Vertex, Edge, Face, Piece };
const char* selectModeName(SelectMode m);

enum class Tool { Select };

// The 2x2 views area: split position, and which view (if any) fills it.
struct ViewsLayout {
    float splitX = 0.5f, splitY = 0.5f;
    int maximised = -1;
};

class Editor {
public:
    Editor(GLFWwindow* window, const EditorOptions& opt);
    ~Editor();

    // One frame of UI. Returns true while something needs continuous frames
    // (flying, dragging a view, a script running).
    bool frame(double dt);
    bool shouldExit() const { return quit_; }
    int exitCode() const { return scriptFailures_ > 0 ? 1 : 0; }
    void requestQuit() { quit_ = true; }

    // A physical key press (from the GLFW callback or a script).
    void keyPressed(int glfwKey, bool shift, bool cmd);

    // Screenshot wanted after this frame is drawn (empty = none). Clears the request.
    std::string takeScreenshotRequest();
    // Pixels per logical pixel for the views (MAZZ_VIEW_SCALE overrides, for tests).
    double viewScale() const;

    // Moves every panel that lives in its own window back over the main window (it then
    // merges into it). Used when a display goes away.
    void bringWindowsHome();

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
    void toolsPanel();           // ui/panels.cpp
    void propertiesPanel();      // ui/panels.cpp
    void partsPanel();           // ui/panels.cpp
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
    SelectMode mode_ = SelectMode::Face;
    Tool tool_ = Tool::Select;
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
    bool bringHome_ = false;
    bool paintWanted_ = false, paintOpen_ = false;
    std::string paintShot_;
    double gridDialogValue_ = 8;

    std::deque<Chord> keyQueue_;
    std::string screenshotRequest_;

    // scripting
    std::deque<std::string> script_;
    int scriptLine_ = 0;
    int scriptWait_ = 0;
    int scriptFailures_ = 0;
    bool scriptMode_ = false;
    int startupFrames_ = 0;
};

} // namespace mazz
