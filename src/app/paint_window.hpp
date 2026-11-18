#pragma once
// The paint window: a second, real app window (its own OS window and its own UI), meant
// for a second screen or an iPad through Sidecar. Same program as the main window, so it
// shares the model, textures and undo with no syncing.
//
// Works on an iPad through Sidecar (with the Apple Pencil) since v0.0.6. For now it's a
// shell: pen-sized tool and palette buttons (not wired yet), a 3D view to orbit by
// dragging, and the pen readout. Painting comes in milestone 6, the full paint view in
// milestone 8.
#include "core/grid.hpp"
#include "render/view_draw.hpp"
#include <string>

struct GLFWwindow;
struct ImGuiContext;

namespace mazz {

class PaintWindow {
public:
    PaintWindow() = default;
    ~PaintWindow() { close(); }
    PaintWindow(const PaintWindow&) = delete;
    PaintWindow& operator=(const PaintWindow&) = delete;

    // Opens it next to the main window, sharing its OpenGL objects. False on failure.
    bool open(GLFWwindow* mainWindow, bool automated);
    void close();
    bool isOpen() const { return window_ != nullptr; }
    // The user closed it with its close button.
    bool wantsClose() const;

    // Draws one frame (makes its own context current, then restores the main one).
    // Returns true while it needs continuous frames (a drag in progress).
    bool frame(const Grid& grid, double dt, GLFWwindow* mainWindow, ImGuiContext* mainCtx);

    // Screenshot of this window after its next frame (for scripted tests).
    void requestScreenshot(const std::string& path) { shot_ = path; }

private:
    void ui(const Grid& grid);
    GLFWwindow* window_ = nullptr;
    ImGuiContext* ctx_ = nullptr;
    View view_;
    ViewDrawer* drawer_ = nullptr; // created in this window's GL context
    int tool_ = 0;
    int colour_ = 0;
    bool dragging_ = false;
    double lastGridSize_ = -1;
    bool lastGridVisible_ = true;
    std::string shot_;
};

} // namespace mazz
