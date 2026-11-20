#pragma once
// Coloured lines and triangles in world space, drawn with one small shader.
// A view collects its geometry into a Batch each time it redraws, then draws it.
#include "core/math.hpp"
#include <cstdint>
#include <vector>

namespace mazz {

struct Rgba {
    uint8_t r, g, b, a;
};

struct ColorVertex {
    float x, y, z;
    uint8_t r, g, b, a;
};

class Batch {
public:
    void clear() { lines_.clear(); tris_.clear(); }
    void line(const Vec3& a, const Vec3& b, Rgba c) { line(a, b, c, c); }
    void line(const Vec3& a, const Vec3& b, Rgba ca, Rgba cb);
    void tri(const Vec3& a, const Vec3& b, const Vec3& c, Rgba col);
    const std::vector<ColorVertex>& lines() const { return lines_; }
    const std::vector<ColorVertex>& tris() const { return tris_; }

private:
    std::vector<ColorVertex> lines_, tris_;
};

// GPU side: the shader and a streaming vertex buffer. Needs a current GL context.
class BatchRenderer {
public:
    BatchRenderer();
    ~BatchRenderer();
    BatchRenderer(const BatchRenderer&) = delete;
    BatchRenderer& operator=(const BatchRenderer&) = delete;

    bool ok() const { return program_ != 0; }
    // Draws triangles first, then lines, with the given view-projection matrix.
    // `depthWrite` false: triangles don't hide what is drawn after them (see-through).
    void draw(const Batch& batch, const Mat4& viewProj, bool depthTest, bool depthWrite = true);

private:
    void upload(const std::vector<ColorVertex>& v);
    unsigned program_ = 0, vao_ = 0, vbo_ = 0;
    int locMvp_ = -1;
};

} // namespace mazz
