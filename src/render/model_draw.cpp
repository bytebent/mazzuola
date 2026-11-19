#include "render/model_draw.hpp"
#include "render/gl.hpp"
#include "render/view_draw.hpp" // view colours
#include <cstddef>
#include <cstdio>

namespace mazz {

namespace {
const char* kVertex = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMvp;
uniform vec3 uLight;
uniform float uLit;
uniform vec4 uColor;
out vec4 vColor;
void main() {
    float shade = mix(1.0, 0.55 + 0.45 * max(dot(normalize(aNormal), uLight), 0.0), uLit);
    vColor = vec4(uColor.rgb * shade, uColor.a);
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
        std::fprintf(stderr, "Mazzuola: model shader error: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

void attribs() {
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DrawVertex), (void*)offsetof(DrawVertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(DrawVertex), (void*)offsetof(DrawVertex, nx));
}

constexpr float c(int v) { return v / 255.0f; }
} // namespace

ModelRenderer::ModelRenderer() {
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
    locColor_ = glGetUniformLocation(program_, "uColor");
    locLight_ = glGetUniformLocation(program_, "uLight");
    locLit_ = glGetUniformLocation(program_, "uLit");

    glGenVertexArrays(1, &triVao_);
    glGenBuffers(1, &triVbo_);
    glGenBuffers(1, &selTriEbo_);
    glBindVertexArray(triVao_);
    glBindBuffer(GL_ARRAY_BUFFER, triVbo_);
    attribs();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, selTriEbo_); // part of the VAO

    glGenVertexArrays(1, &edgeVao_);
    glGenBuffers(1, &edgeVbo_);
    glGenBuffers(1, &selEdgeEbo_);
    glBindVertexArray(edgeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, edgeVbo_);
    attribs();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, selEdgeEbo_);
    glBindVertexArray(0);
}

ModelRenderer::~ModelRenderer() {
    if (!program_) return;
    glDeleteBuffers(1, &triVbo_);
    glDeleteBuffers(1, &edgeVbo_);
    glDeleteBuffers(1, &selTriEbo_);
    glDeleteBuffers(1, &selEdgeEbo_);
    glDeleteVertexArrays(1, &triVao_);
    glDeleteVertexArrays(1, &edgeVao_);
    glDeleteProgram(program_);
}

void ModelRenderer::setGeometry(const DrawData& d) {
    if (!program_) return;
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, triVbo_);
    glBufferData(GL_ARRAY_BUFFER, (long)(d.tris.size() * sizeof(DrawVertex)), d.tris.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, edgeVbo_);
    glBufferData(GL_ARRAY_BUFFER, (long)(d.edges.size() * sizeof(DrawVertex)), d.edges.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    triCount_ = (int)d.tris.size();
    edgeCount_ = (int)d.edges.size();
}

void ModelRenderer::setSelection(const std::vector<unsigned>& tris, const std::vector<unsigned>& edges) {
    if (!program_) return;
    glBindVertexArray(triVao_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (long)(tris.size() * sizeof(unsigned)), tris.data(), GL_DYNAMIC_DRAW);
    glBindVertexArray(edgeVao_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (long)(edges.size() * sizeof(unsigned)), edges.data(), GL_DYNAMIC_DRAW);
    glBindVertexArray(0);
    selTriCount_ = (int)tris.size();
    selEdgeCount_ = (int)edges.size();
}

void ModelRenderer::use(const Mat4& mvp, float r, float g, float b, float a, bool lit, const Vec3& toLight) {
    glUseProgram(program_);
    glUniformMatrix4fv(locMvp_, 1, GL_FALSE, mvp.m);
    glUniform4f(locColor_, r, g, b, a);
    const Vec3 l = normalize(toLight);
    glUniform3f(locLight_, (float)l.x, (float)l.y, (float)l.z);
    glUniform1f(locLit_, lit ? 1.0f : 0.0f);
}

void ModelRenderer::draw3D(const Mat4& viewProj, const Vec3& toLight) {
    if (!program_ || triCount_ == 0) return;
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    // Faces pushed back a little so edges and the selection tint draw on top of them.
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    use(viewProj, c(232), c(233), c(236), 1, true, toLight);
    glBindVertexArray(triVao_);
    glDrawArrays(GL_TRIANGLES, 0, triCount_);
    if (selTriCount_) {
        glPolygonOffset(0.5f, 0.5f);
        const Rgba s = viewcol::kSelection;
        use(viewProj, c(s.r) * 0.6f + 0.4f, c(s.g) * 0.6f + 0.4f, c(s.b) * 0.6f + 0.4f, 1, true, toLight);
        glDrawElements(GL_TRIANGLES, selTriCount_, GL_UNSIGNED_INT, nullptr);
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    use(viewProj, c(96), c(100), c(110), 1, false);
    glBindVertexArray(edgeVao_);
    glDrawArrays(GL_LINES, 0, edgeCount_);
    if (selEdgeCount_) {
        const Rgba s = viewcol::kSelection;
        use(viewProj, c(s.r), c(s.g), c(s.b), 1, false);
        glDrawElements(GL_LINES, selEdgeCount_, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
    glDisable(GL_DEPTH_TEST);
}

void ModelRenderer::draw2D(const Mat4& viewProj) {
    if (!program_ || edgeCount_ == 0) return;
    glDisable(GL_DEPTH_TEST);
    use(viewProj, c(40), c(44), c(52), 1, false);
    glBindVertexArray(edgeVao_);
    glDrawArrays(GL_LINES, 0, edgeCount_);
    if (selEdgeCount_) {
        const Rgba s = viewcol::kSelection;
        use(viewProj, c(s.r), c(s.g), c(s.b), 1, false);
        glDrawElements(GL_LINES, selEdgeCount_, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
}

} // namespace mazz
