#include "render/view_target.hpp"
#include "render/gl.hpp"
#include <cstdio>
#include <cstdlib>

namespace mazz {

namespace {
constexpr int kSamples = 4;

// MAZZ_VIEW_SAMPLES=0 forces the plain (not anti-aliased) path, so tests can run it.
bool multisampleAllowed() {
    const char* s = std::getenv("MAZZ_VIEW_SAMPLES");
    return !(s && s[0] == '0' && s[1] == 0);
}
}

ViewTarget::~ViewTarget() { release(); }

void ViewTarget::release() {
    releaseMultisample();
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (depth_) glDeleteRenderbuffers(1, &depth_);
    if (tex_) glDeleteTextures(1, &tex_);
    fbo_ = depth_ = tex_ = 0;
    w_ = h_ = 0;
}

void ViewTarget::releaseMultisample() {
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    if (msColor_) glDeleteRenderbuffers(1, &msColor_);
    if (msDepth_) glDeleteRenderbuffers(1, &msDepth_);
    msFbo_ = msColor_ = msDepth_ = 0;
}

bool ViewTarget::resize(int pxW, int pxH) {
    if (pxW < 1) pxW = 1;
    if (pxH < 1) pxH = 1;
    if (pxW == w_ && pxH == h_ && tex_) return false;
    release();
    w_ = pxW;
    h_ = pxH;

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo_);

    // The texture the UI shows. Also the drawing surface when anti-aliasing is unavailable.
    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w_, h_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_, 0);

    // Anti-aliased surface (smooth lines). OpenGL 3.2 guarantees 4 samples, but the
    // buffers can still fail (GPU memory runs out, a huge view): then draw straight
    // into the texture instead, without anti-aliasing, rather than show a blank view.
    bool multisampled = false;
    if (multisampleAllowed()) {
        glGenRenderbuffers(1, &msColor_);
        glBindRenderbuffer(GL_RENDERBUFFER, msColor_);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, kSamples, GL_RGBA8, w_, h_);
        glGenRenderbuffers(1, &msDepth_);
        glBindRenderbuffer(GL_RENDERBUFFER, msDepth_);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, kSamples, GL_DEPTH_COMPONENT24, w_, h_);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glGenFramebuffers(1, &msFbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth_);
        multisampled = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        if (!multisampled) {
            std::fprintf(stderr, "Mazzuola: anti-aliased view buffer unavailable, drawing without\n");
            glBindFramebuffer(GL_FRAMEBUFFER, (unsigned)prevFbo_);
            releaseMultisample();
        }
    }
    if (!multisampled) {
        glGenRenderbuffers(1, &depth_);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w_, h_);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::fprintf(stderr, "Mazzuola: view framebuffer incomplete\n");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, (unsigned)prevFbo_);
    return true;
}

void ViewTarget::begin(float r, float g, float b) {
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_ ? msFbo_ : fbo_);
    glViewport(0, 0, w_, h_);
    glClearColor(r, g, b, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void ViewTarget::end() {
    if (!msFbo_) { // drew straight into the texture
        glBindFramebuffer(GL_FRAMEBUFFER, (unsigned)prevFbo_);
        return;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_);
    glBlitFramebuffer(0, 0, w_, h_, 0, 0, w_, h_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, (unsigned)prevFbo_);
}

} // namespace mazz
