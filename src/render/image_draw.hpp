#pragma once
// Pictures in the views (reference images): an RGBA texture drawn on a quad in world
// space with a transparency. Needs a current GL context.
#include "core/math.hpp"

namespace mazz {

// Uploads RGBA pixels (top row first) as a smooth-filtered texture; 0 on failure.
unsigned uploadTexture(const unsigned char* rgba, int w, int h);
void deleteTexture(unsigned tex);

class ImageRenderer {
public:
    ImageRenderer();
    ~ImageRenderer();
    ImageRenderer(const ImageRenderer&) = delete;
    ImageRenderer& operator=(const ImageRenderer&) = delete;

    // Corners: the picture's bottom-left, bottom-right, top-right, top-left.
    void draw(unsigned tex, const Vec3 corners[4], float alpha, const Mat4& viewProj);

private:
    unsigned program_ = 0, vao_ = 0, vbo_ = 0;
    int locMvp_ = -1, locAlpha_ = -1;
};

} // namespace mazz
