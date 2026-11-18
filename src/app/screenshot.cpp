#include "app/screenshot.hpp"
#include "render/gl.hpp"
#include <GLFW/glfw3.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace mazz {

void saveScreenshot(GLFWwindow* w, const std::string& path) {
    int fw = 0, fh = 0;
    glfwGetFramebufferSize(w, &fw, &fh);
    std::vector<unsigned char> px((size_t)fw * fh * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, fw, fh, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    std::vector<unsigned char> flipped(px.size());
    for (int y = 0; y < fh; ++y)
        std::memcpy(&flipped[(size_t)y * fw * 4], &px[(size_t)(fh - 1 - y) * fw * 4], (size_t)fw * 4);
    for (size_t i = 3; i < flipped.size(); i += 4) flipped[i] = 255;
    if (!stbi_write_png(path.c_str(), fw, fh, 4, flipped.data(), fw * 4))
        std::fprintf(stderr, "Mazzuola: could not write %s\n", path.c_str());
}

} // namespace mazz
