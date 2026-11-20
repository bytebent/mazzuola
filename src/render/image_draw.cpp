#include "render/image_draw.hpp"
#include "render/gl.hpp"
#include <cstdio>

namespace mazz {

namespace {
const char* kVertex = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
uniform mat4 uMvp;
out vec2 vUv;
void main() {
    vUv = aUv;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)";

// The sampler stays on texture unit 0 (its default).
const char* kFragment = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uTex;
uniform float uAlpha;
out vec4 fragColor;
void main() {
    vec4 c = texture(uTex, vUv);
    fragColor = vec4(c.rgb, c.a * uAlpha);
}
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
        std::fprintf(stderr, "Mazzuola: image shader error: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}
} // namespace

unsigned uploadTexture(const unsigned char* rgba, int w, int h) {
    if (!rgba || w <= 0 || h <= 0) return 0;
    unsigned tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void deleteTexture(unsigned tex) {
    if (tex) glDeleteTextures(1, &tex);
}

ImageRenderer::ImageRenderer() {
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
        glDeleteProgram(program_);
        program_ = 0;
        return;
    }
    locMvp_ = glGetUniformLocation(program_, "uMvp");
    locAlpha_ = glGetUniformLocation(program_, "uAlpha");
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

ImageRenderer::~ImageRenderer() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (program_) glDeleteProgram(program_);
}

void ImageRenderer::draw(unsigned tex, const Vec3 c[4], float alpha, const Mat4& viewProj) {
    if (!program_ || !tex) return;
    // Texture rows are top first, so the picture's top is t = 0.
    const float uv[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    const int order[6] = {0, 1, 2, 0, 2, 3};
    float v[30];
    for (int i = 0; i < 6; ++i) {
        const int k = order[i];
        v[5 * i + 0] = (float)c[k].x;
        v[5 * i + 1] = (float)c[k].y;
        v[5 * i + 2] = (float)c[k].z;
        v[5 * i + 3] = uv[k][0];
        v[5 * i + 4] = uv[k][1];
    }
    glUseProgram(program_);
    glUniformMatrix4fv(locMvp_, 1, GL_FALSE, viewProj.m);
    glUniform1f(locAlpha_, alpha);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof v, v, GL_STREAM_DRAW);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

} // namespace mazz
