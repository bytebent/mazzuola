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

bool writeTestImage(const std::string& path, int w, int h) {
    // A checkerboard of 8 px squares with a red top-left square (to see which way is up).
    std::vector<unsigned char> px((size_t)w * h * 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            unsigned char* p = &px[((size_t)y * w + x) * 4];
            const bool dark = ((x / 8) + (y / 8)) % 2;
            const bool mark = x < 8 && y < 8;
            p[0] = mark ? 220 : dark ? 60 : 230;
            p[1] = mark ? 40 : dark ? 90 : 230;
            p[2] = mark ? 40 : dark ? 140 : 230;
            p[3] = 255;
        }
    return stbi_write_png(path.c_str(), w, h, 4, px.data(), w * 4) != 0;
}

} // namespace mazz
