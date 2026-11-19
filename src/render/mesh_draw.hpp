#pragma once
// Drawing model faces (lit, for the 3D view) and textured quads (reference images).
// Needs a current GL context; one of each per context.
#include "core/math.hpp"
#include <cstdint>
#include <string>
#include <map>
#include <vector>

namespace mazz {

struct LitVertex {
    float x, y, z;
    float nx, ny, nz;
    uint8_t r, g, b, a;
};

class LitRenderer {
public:
    LitRenderer();
    ~LitRenderer();
    LitRenderer(const LitRenderer&) = delete;
    LitRenderer& operator=(const LitRenderer&) = delete;
    // Triangles lit by a light from the camera (`toEye` points from the scene to the
    // camera). Faces are pushed back slightly so lines drawn afterwards sit on top.
    void draw(const std::vector<LitVertex>& tris, const Mat4& viewProj, const Vec3& toEye);

private:
    unsigned program_ = 0, vao_ = 0, vbo_ = 0;
    int locMvp_ = -1, locLight_ = -1;
};

// A picture drawn as a quad in the world (corners counter-clockwise from bottom-left).
class ImageRenderer {
public:
    ImageRenderer();
    ~ImageRenderer();
    ImageRenderer(const ImageRenderer&) = delete;
    ImageRenderer& operator=(const ImageRenderer&) = delete;
    void draw(unsigned texture, const Vec3 corners[4], float alpha, const Mat4& viewProj);

private:
    unsigned program_ = 0, vao_ = 0, vbo_ = 0;
    int locMvp_ = -1, locAlpha_ = -1, locTex_ = -1;
};

// Images loaded from disk (PNG, JPG, TGA, BMP) as GL textures, kept by path.
class TextureCache {
public:
    struct Entry {
        unsigned tex = 0;
        int w = 0, h = 0;
        bool failed = false;
    };
    ~TextureCache();
    // Loads on first use. A file that can't be read gives an entry with failed = true.
    const Entry& get(const std::string& path);
    void forget(const std::string& path);
    void clear();

private:
    std::map<std::string, Entry> entries_;
};

unsigned compileProgram(const char* vs, const char* fs);

} // namespace mazz
