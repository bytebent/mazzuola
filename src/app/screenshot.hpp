#pragma once
// Saves a window's back buffer (just drawn, before the swap) as a PNG.
#include <string>

struct GLFWwindow;

namespace mazz {
void saveScreenshot(GLFWwindow* window, const std::string& path);
// Test helper: a w x h checkerboard PNG (top-left square red).
bool writeTestImage(const std::string& path, int w, int h);
}
