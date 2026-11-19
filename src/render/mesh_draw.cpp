#include "render/mesh_draw.hpp"
#include "render/gl.hpp"
#include "stb_image.h"
#include <cstddef>
#include <cstdio>

namespace mazz {

unsigned compileProgram(const char* vsSrc, const char* fsSrc) {
    auto compile = [](unsigned type, const char* src) -> unsigned {
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
    };
    const unsigned vs = compile(GL_VERTEX_SHADER, vsSrc), fs = compile(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) return 0;
    const unsigned p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    int ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof log, nullptr, log);
        std::fprintf(stderr, "Mazzuola: shader link error: %s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

// ---------------------------------------------------------------- lit faces

namespace {
const char* kLitVs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;
uniform mat4 uMvp;
out vec3 vNormal;
out vec4 vColor;
void main() {
    vNormal = aNormal;
    vColor = aColor;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)";
const char* kLitFs = R"(#version 330 core
in vec3 vNormal;
in vec4 vColor;
uniform vec3 uToEye;
out vec4 fragColor;
void main() {
    vec3 n = normalize(vNormal);
    // Back faces (seen from behind) are shown darker.
    float facing = dot(n, uToEye);
    float light = 0.42 + 0.58 * abs(facing);
    if (!gl_FrontFacing) light *= 0.55;
    fragColor = vec4(vColor.rgb * light, vColor.a);
}
)";
const char* kImgVs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
uniform mat4 uMvp;
out vec2 vUv;
void main() {
    vUv = aUv;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)";
const char* kImgFs = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uTex;
uniform float uAlpha;
out vec4 fragColor;
void main() {
    vec4 c = texture(uTex, vUv);
    fragColor = vec4(c.rgb, c.a * uAlpha);
}
)";
} // namespace

LitRenderer::LitRenderer() {
    program_ = compileProgram(kLitVs, kLitFs);
    if (!program_) return;
    locMvp_ = glGetUniformLocation(program_, "uMvp");
    locLight_ = glGetUniformLocation(program_, "uToEye");
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LitVertex), (void*)offsetof(LitVertex, x));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LitVertex), (void*)offsetof(LitVertex, nx));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(LitVertex), (void*)offsetof(LitVertex, r));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

LitRenderer::~LitRenderer() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (program_) glDeleteProgram(program_);
}

void LitRenderer::draw(const std::vector<LitVertex>& tris, const Mat4& viewProj, const Vec3& toEye) {
    if (!program_ || tris.empty()) return;
    glUseProgram(program_);
    glUniformMatrix4fv(locMvp_, 1, GL_FALSE, viewProj.m);
    const Vec3 l = normalize(toEye);
    glUniform3f(locLight_, (float)l.x, (float)l.y, (float)l.z);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, (long)(tris.size() * sizeof(LitVertex)), tris.data(), GL_STREAM_DRAW);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    glDisable(GL_BLEND);
    glDrawArrays(GL_TRIANGLES, 0, (int)tris.size());
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindVertexArray(0);
    glUseProgram(0);
}

// ---------------------------------------------------------------- images

ImageRenderer::ImageRenderer() {
    program_ = compileProgram(kImgVs, kImgFs);
    if (!program_) return;
    locMvp_ = glGetUniformLocation(program_, "uMvp");
    locAlpha_ = glGetUniformLocation(program_, "uAlpha");
    locTex_ = glGetUniformLocation(program_, "uTex");
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

void ImageRenderer::draw(unsigned texture, const Vec3 c[4], float alpha, const Mat4& viewProj) {
    if (!program_ || !texture) return;
    // Two triangles; image rows are stored top-first, so v = 1 at the bottom corners.
    const float uv[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    const int order[6] = {0, 1, 2, 0, 2, 3};
    float data[6 * 5];
    for (int i = 0; i < 6; ++i) {
        const Vec3& p = c[order[i]];
        data[i * 5 + 0] = (float)p.x;
        data[i * 5 + 1] = (float)p.y;
        data[i * 5 + 2] = (float)p.z;
        data[i * 5 + 3] = uv[order[i]][0];
        data[i * 5 + 4] = uv[order[i]][1];
    }
    glUseProgram(program_);
    glUniformMatrix4fv(locMvp_, 1, GL_FALSE, viewProj.m);
    glUniform1f(locAlpha_, alpha);
    glUniform1i(locTex_, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof data, data, GL_STREAM_DRAW);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

// ---------------------------------------------------------------- texture cache

TextureCache::~TextureCache() { clear(); }

const TextureCache::Entry& TextureCache::get(const std::string& path) {
    auto it = entries_.find(path);
    if (it != entries_.end()) return it->second;
    Entry e;
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!px) {
        e.failed = true;
    } else {
        glGenTextures(1, &e.tex);
        glBindTexture(GL_TEXTURE_2D, e.tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        glGenerateMipmap(GL_TEXTURE_2D);
        // Reference photos: smooth when zoomed out, smooth when zoomed in.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(px);
        e.w = w;
        e.h = h;
    }
    return entries_[path] = e;
}

void TextureCache::forget(const std::string& path) {
    auto it = entries_.find(path);
    if (it == entries_.end()) return;
    if (it->second.tex) glDeleteTextures(1, &it->second.tex);
    entries_.erase(it);
}

void TextureCache::clear() {
    for (auto& [p, e] : entries_)
        if (e.tex) glDeleteTextures(1, &e.tex);
    entries_.clear();
}

} // namespace mazz
