// Mazzuola — entry point and main loop.
//
// Idle policy: the app sleeps in glfwWaitEvents whenever nothing is happening. After
// any event it renders a few "settle" frames so ImGui can finish layout changes, then
// sleeps again. It runs continuously only while the editor says it is busy (flying,
// dragging a view, scripted runs).
#include "app/editor.hpp"
#include "app/paint_window.hpp"
#include "app/screenshot.hpp"
#include "app/settings.hpp"
#include "platform/pen.hpp"
#include "render/gl.hpp"
#include "ui/theme.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace mazz;

extern const unsigned char mazz_font_roboto[];
extern const unsigned int mazz_font_roboto_size;

namespace {
constexpr int kSettleFrames = 3;
Editor* gEditor = nullptr;

EditorOptions parseArgs(int argc, char** argv) {
    EditorOptions o;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], "--script") == 0) o.scriptPath = argv[++i];
        else if (std::strcmp(argv[i], "--screenshot") == 0) o.screenshot = argv[++i];
    }
    return o;
}

void onKey(GLFWwindow*, int key, int, int action, int mods) {
    if (action != GLFW_PRESS || !gEditor) return;
#if defined(__APPLE__)
    const bool cmd = (mods & GLFW_MOD_SUPER) != 0;
#else
    const bool cmd = (mods & GLFW_MOD_CONTROL) != 0;
#endif
    gEditor->keyPressed(key, (mods & GLFW_MOD_SHIFT) != 0, cmd);
}

// A display was added or removed (e.g. Sidecar turned off): ask the editor to bring
// panels that live in their own windows back into the main window.
bool gMonitorsChanged = false;
GLFWmonitorfun gPrevMonitorCallback = nullptr; // ImGui's own (keeps its monitor list current)
void onMonitor(GLFWmonitor* m, int event) {
    if (event == GLFW_DISCONNECTED) gMonitorsChanged = true;
    if (gPrevMonitorCallback) gPrevMonitorCallback(m, event);
}

void onError(int code, const char* text) { std::fprintf(stderr, "Mazzuola: GLFW error %d: %s\n", code, text); }

} // namespace

int main(int argc, char** argv) {
    const EditorOptions opt = parseArgs(argc, argv);

    glfwSetErrorCallback(onError);
    if (!glfwInit()) return 2;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(1440, 900, "Mazzuola", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Mazzuola: could not open an OpenGL 4.1 window.\n");
        glfwTerminate();
        return 2;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    const char* missing = nullptr;
    if (!loadGL([](const char* n) { return (void*)glfwGetProcAddress(n); }, &missing)) {
        std::fprintf(stderr, "Mazzuola: OpenGL function missing: %s\n", missing ? missing : "?");
        return 2;
    }

    // Our key callback first; ImGui's backend chains to it.
    glfwSetKeyCallback(window, onKey);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // Windows move only by their title bar: a click or pen touch on a panel's empty
    // space must never drag the window around.
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    // Panels can be pulled out into their own windows (e.g. onto a second screen).
    // Automated runs keep everything in one window.
    if (!opt.automated()) io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    static std::string iniPath;
    if (std::string dir = settingsDir(); !dir.empty() && !opt.automated()) {
        iniPath = dir + "/layout.ini";
        io.IniFilename = iniPath.c_str();
    } else {
        io.IniFilename = nullptr; // automated runs never touch user settings
    }
    applyTheme();
    {
        ImFontConfig cfg;
        cfg.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF((void*)mazz_font_roboto, (int)mazz_font_roboto_size, 15.0f, &cfg);
    }
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    gPrevMonitorCallback = glfwSetMonitorCallback(onMonitor);
    installPenInput();
    ImGuiContext* mainCtx = ImGui::GetCurrentContext();
    registerPenContext(mainCtx);

    int exitCode = 0;
    {
        Editor editor(window, opt);
        PaintWindow paint;
        gEditor = &editor;
        int settle = kSettleFrames;
        bool busy = true;
        double last = glfwGetTime();
        while (!editor.shouldExit()) {
            if (busy || settle > 0) glfwPollEvents();
            else glfwWaitEvents();
            if (glfwWindowShouldClose(window)) editor.requestQuit();

            const double now = glfwGetTime();
            const double dt = std::min(0.1, now - last);
            last = now;

            if (gMonitorsChanged) {
                gMonitorsChanged = false;
                editor.bringWindowsHome();
            }
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            busy = editor.frame(dt);
            ImGui::Render();

            int fw = 0, fh = 0;
            glfwGetFramebufferSize(window, &fw, &fh);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, fw, fh);
            glClearColor(0.955f, 0.957f, 0.965f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            const std::string shot = editor.takeScreenshotRequest();
            if (!shot.empty()) saveScreenshot(window, shot);

            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                glfwMakeContextCurrent(window);
            }
            glfwSwapBuffers(window);

            // The paint window: open/close as asked, then draw it.
            if (paint.wantsClose()) {
                paint.close();
                editor.paintWindowClosed();
            }
            if (editor.paintWindowWanted() && !paint.isOpen() && !paint.open(window, opt.automated()))
                editor.paintWindowClosed();
            if (!editor.paintWindowWanted() && paint.isOpen()) paint.close();
            editor.setPaintWindowOpen(paint.isOpen());
            if (paint.isOpen()) {
                const std::string pshot = editor.takePaintShot();
                if (!pshot.empty()) paint.requestScreenshot(pshot);
                if (paint.frame(editor.grid(), dt, window, mainCtx)) busy = true;
            }

            if (busy) settle = kSettleFrames;
            else if (settle > 0) --settle;
            else settle = kSettleFrames; // after the wait returns (an event), settle again
        }
        paint.close();
        exitCode = editor.exitCode();
        gEditor = nullptr;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return exitCode;
}
