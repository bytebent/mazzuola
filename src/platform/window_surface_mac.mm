#import <AppKit/AppKit.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#define GLFW_EXPOSE_NATIVE_NSGL
#include <GLFW/glfw3native.h>
#include "platform/window_surface.hpp"

#pragma clang diagnostic ignored "-Wdeprecated-declarations" // NSOpenGL (OpenGL is deprecated on macOS)

namespace mazz {

void refreshWindowSurface(GLFWwindow* window, bool updateContext, bool redisplay) {
    if (!window) return;
    if (updateContext) {
        if (NSOpenGLContext* ctx = glfwGetNSGLContext(window)) [ctx update];
    }
    if (redisplay) {
        if (NSWindow* w = glfwGetCocoaWindow(window)) [w.contentView setNeedsDisplay:YES];
    }
}

} // namespace mazz
