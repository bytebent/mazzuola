#include "app/paint_window.hpp"
#include "app/screenshot.hpp"
#include "platform/pen.hpp"
#include "render/gl.hpp"
#include "ui/theme.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

extern const unsigned char mazz_font_roboto[];
extern const unsigned int mazz_font_roboto_size;

namespace mazz {

namespace {
ImTextureID texId(unsigned tex) { return (ImTextureID)(intptr_t)tex; }

// A test palette (the real one arrives in milestone 6).
const ImVec4 kSwatches[] = {
    {0.10f, 0.11f, 0.17f, 1}, {0.36f, 0.15f, 0.36f, 1}, {0.69f, 0.24f, 0.33f, 1}, {0.94f, 0.49f, 0.34f, 1},
    {1.00f, 0.80f, 0.46f, 1}, {0.65f, 0.94f, 0.44f, 1}, {0.22f, 0.72f, 0.39f, 1}, {0.15f, 0.44f, 0.47f, 1},
};
} // namespace

bool PaintWindow::open(GLFWwindow* mainWindow, bool automated) {
    if (window_) return true;
    glfwDefaultWindowHints(); // ImGui changes hints when it creates its own windows
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_COCOA_GRAPHICS_SWITCHING, GLFW_TRUE); // needed on Sidecar, see main.cpp
    window_ = glfwCreateWindow(900, 700, "Mazzuola - Paint", nullptr, mainWindow);
    if (!window_) return false;
    int mx = 0, my = 0;
    glfwGetWindowPos(mainWindow, &mx, &my);
    glfwSetWindowPos(window_, mx + 60, my + 60);

    ImGuiContext* prevCtx = ImGui::GetCurrentContext();
    glfwMakeContextCurrent(window_);
    // Waits for the display like the main window. (While both windows draw, each waits:
    // to optimise in milestone 8 by drawing only the window that changed.)
    glfwSwapInterval(1);
    ctx_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    applyTheme();
    // Pen-sized: bigger text and touch targets.
    ImGuiStyle& s = ImGui::GetStyle();
    s.FramePadding = ImVec2(12, 10);
    s.ItemSpacing = ImVec2(10, 10);
    s.WindowPadding = ImVec2(14, 14);
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF((void*)mazz_font_roboto, (int)mazz_font_roboto_size, 19.0f, &cfg);
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    registerPenContext(ctx_);
    (void)automated;

    view_.setDir(ViewDir::Persp);
    view_.dirty = true;

    ImGui::SetCurrentContext(prevCtx);
    glfwMakeContextCurrent(mainWindow);
    return true;
}

void PaintWindow::close() {
    if (!window_) return;
    GLFWwindow* prevWin = glfwGetCurrentContext();
    ImGuiContext* prevCtx = ImGui::GetCurrentContext();
    glfwMakeContextCurrent(window_);
    // This window's GL objects first, while its context is current.
    delete drawer_;
    drawer_ = nullptr;
    view_.target.release();
    view_.dirty = true;
    ImGui::SetCurrentContext(ctx_);
    unregisterPenContext(ctx_);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(ctx_);
    ImGuiContext* goneCtx = ctx_;
    GLFWwindow* goneWin = window_;
    ctx_ = nullptr;
    window_ = nullptr;
    glfwDestroyWindow(goneWin);
    ImGui::SetCurrentContext(prevCtx == goneCtx ? nullptr : prevCtx);
    glfwMakeContextCurrent(prevWin == goneWin ? nullptr : prevWin);
}

bool PaintWindow::wantsClose() const { return window_ && glfwWindowShouldClose(window_); }

bool PaintWindow::frame(const Grid& grid, const Scene& scene, double dt, GLFWwindow* mainWindow, ImGuiContext* mainCtx) {
    if (!window_) return false;
    glfwMakeContextCurrent(window_);
    ImGui::SetCurrentContext(ctx_);
    if (!drawer_) drawer_ = new ViewDrawer();
    if (grid.size != lastGridSize_ || grid.visible != lastGridVisible_) {
        lastGridSize_ = grid.size;
        lastGridVisible_ = grid.visible;
        view_.dirty = true;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ui(grid, scene);
    ImGui::Render();
    int fw = 0, fh = 0;
    glfwGetFramebufferSize(window_, &fw, &fh);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fw, fh);
    glClearColor(0.955f, 0.957f, 0.965f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (!shot_.empty()) {
        saveScreenshot(window_, shot_);
        shot_.clear();
    }
    glfwSwapBuffers(window_);

    ImGui::SetCurrentContext(mainCtx);
    glfwMakeContextCurrent(mainWindow);
    (void)dt;
    return dragging_;
}

void PaintWindow::ui(const Grid& grid, const Scene& scene) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    const ImGuiWindowFlags full = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##paint", nullptr, full);

    // ---- left strip: tools, palette, tap test
    const float stripW = 230;
    ImGui::BeginChild("##strip", ImVec2(stripW, 0));
    ImGui::TextDisabled("Painting arrives in milestone 6");
    static const char* tools[] = {"Pencil", "Fill", "Picker", "Eraser"};
    for (int i = 0; i < 4; ++i) {
        const bool on = tool_ == i;
        if (on) {
            ImGui::PushStyleColor(ImGuiCol_Button, theme::kAccent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::kAccentHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::kAccentActive);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        }
        if (ImGui::Button(tools[i], ImVec2(-1, 0))) tool_ = i;
        if (on) ImGui::PopStyleColor(4);
    }
    ImGui::Spacing();
    ImGui::TextDisabled("Palette");
    for (int i = 0; i < 8; ++i) {
        if (i % 4) ImGui::SameLine();
        ImGui::PushID(i);
        const float side = (stripW - ImGui::GetStyle().ItemSpacing.x * 3) / 4 - 2;
        if (ImGui::ColorButton("##sw", kSwatches[i], ImGuiColorEditFlags_NoTooltip, ImVec2(side, side))) colour_ = i;
        if (colour_ == i) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(theme::kAccent), 3.0f, 0, 3.0f);
        }
        ImGui::PopID();
    }
    ImGui::Spacing();
    const ImGuiIO& io = ImGui::GetIO();
    const PenState& p = penState();
    ImGui::TextDisabled("Pen");
    ImGui::Text("Source: %s", io.MouseSource == ImGuiMouseSource_Pen ? "pen" : io.MouseSource == ImGuiMouseSource_TouchScreen ? "touch" : "mouse");
    ImGui::Text("Pressure: %.2f", p.pressure);
    ImGui::Text("Tilt: %.2f, %.2f", p.tiltX, p.tiltY);
    ImGui::TextWrapped("Drag on the view to turn it; scroll (two fingers) to zoom.");
    ImGui::EndChild();

    // ---- the view: drag orbits (pens have no right button), scroll zooms
    ImGui::SameLine();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::GetContentRegionAvail();
    const double scale = io.DisplayFramebufferScale.x > 0 ? io.DisplayFramebufferScale.x : 1.0;
    drawer_->draw(view_, grid, scene, std::max(1.0f, size.x), std::max(1.0f, size.y), scale);
    ImGui::InvisibleButton("##view", ImVec2(std::max(1.0f, size.x), std::max(1.0f, size.y)));
    ImGui::GetWindowDrawList()->AddImage(texId(view_.target.texture()), pos, {pos.x + size.x, pos.y + size.y}, {0, 1}, {1, 0});
    dragging_ = ImGui::IsItemActive();
    if (dragging_ && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) {
        view_.cam3d.orbit(-io.MouseDelta.x * 0.35, io.MouseDelta.y * 0.35);
        view_.dirty = true;
    }
    if (ImGui::IsItemHovered() && io.MouseWheel != 0) {
        view_.cam3d.dolly(std::pow(0.87, (double)io.MouseWheel));
        view_.dirty = true;
    }
    ImGui::End();
}

} // namespace mazz
