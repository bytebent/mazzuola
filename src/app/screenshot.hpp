#pragma once
// Saves a window's back buffer (just drawn, before the swap) as a PNG.
#include <string>

struct GLFWwindow;

namespace mazz {
void saveScreenshot(GLFWwindow* window, const std::string& path);
}
