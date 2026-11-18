#pragma once
// macOS: ways to make sure a window's new picture reaches the screen (tested for the
// paint window on a Sidecar display). Elsewhere: no-ops.
struct GLFWwindow;

namespace mazz {
// updateContext: tell the window's OpenGL context to re-check its window and display.
// redisplay: mark the window's view as needing to be shown again.
void refreshWindowSurface(GLFWwindow* window, bool updateContext, bool redisplay);
}
