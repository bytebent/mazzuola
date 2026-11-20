#include "render/lines.hpp"
#include "render/gl.hpp"
#include <cstddef>
#include <cstdio>

namespace mazz {

void Batch::line(const Vec3& a, const Vec3& b, Rgba ca, Rgba cb) {
    lines_.push_back({(float)a.x, (float)a.y, (float)a.z, ca.r, ca.g, ca.b, ca.a});
    lines_.push_back({(float)b.x, (float)b.y, (float)b.z, cb.r, cb.g, cb.b, cb.a});
}

void Batch::tri(const Vec3& a, const Vec3& b, const Vec3& c, Rgba col) {
    for (const Vec3* p : {&a, &b, &c})
        tris_.push_back({(float)p->x, (float)p->y, (float)p->z, col.r, col.g, col.b, col.a});
}

namespace {
const char* kVertex = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
uniform mat4 uMvp;
out vec4 vColor;
void main() {
    vColor = aColor;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

const char* kFragment = R"(#version 330 core
in vec4 vColor;
out vec4 fragColor;
void main() { fragColor = vColor; }
)";

unsigned compile(unsigned type, const char* src) {
    const unsigned s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        std::fprintf(stderr, "Mazzuola: shader error: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}
} // namespace

BatchRenderer::BatchRenderer() {
    const unsigned vs = compile(GL_VERTEX_SHADER, kVertex);
    const unsigned fs = compile(GL_FRAGMENT_SHADER, kFragment);
    if (!vs || !fs) return;
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    int ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program_, sizeof log, nullptr, log);
        std::fprintf(stderr, "Mazzuola: shader link error: %s\n", log);
        glDeleteProgram(program_);
        program_ = 0;
        return;
    }
    locMvp_ = glGetUniformLocation(program_, "uMvp");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), (void*)offsetof(ColorVertex, x));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ColorVertex), (void*)offsetof(ColorVertex, r));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

BatchRenderer::~BatchRenderer() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (program_) glDeleteProgram(program_);
}

void BatchRenderer::upload(const std::vector<ColorVertex>& v) {
    glBufferData(GL_ARRAY_BUFFER, (long)(v.size() * sizeof(ColorVertex)), v.data(), GL_STREAM_DRAW);
}

void BatchRenderer::draw(const Batch& batch, const Mat4& viewProj, bool depthTest, bool depthWrite) {
    if (!program_) return;
    glUseProgram(program_);
    glUniformMatrix4fv(locMvp_, 1, GL_FALSE, viewProj.m);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    if (depthTest) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!batch.tris().empty()) {
        upload(batch.tris());
        if (!depthWrite) glDepthMask(GL_FALSE);
        glDrawArrays(GL_TRIANGLES, 0, (int)batch.tris().size());
        glDepthMask(GL_TRUE);
    }
    if (!batch.lines().empty()) {
        glDepthMask(GL_FALSE); // lines never hide each other
        upload(batch.lines());
        glDrawArrays(GL_LINES, 0, (int)batch.lines().size());
        glDepthMask(GL_TRUE);
    }
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
}

} // namespace mazz
