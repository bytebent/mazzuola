#pragma once
// Offscreen image a view draws into: a multisampled colour + depth buffer (smooth
// lines), resolved into a plain texture that the UI shows. Sized in real pixels.
namespace mazz {

class ViewTarget {
public:
    ViewTarget() = default;
    ~ViewTarget();
    ViewTarget(const ViewTarget&) = delete;
    ViewTarget& operator=(const ViewTarget&) = delete;

    // (Re)creates the buffers if the pixel size changed. Returns true if it did.
    bool resize(int pxW, int pxH);
    int width() const { return w_; }
    int height() const { return h_; }

    // Binds the multisampled buffer and sets the viewport; clears to `rgb` (0..1).
    void begin(float r, float g, float b);
    // Resolves into the texture and unbinds.
    void end();

    unsigned texture() const { return tex_; }

private:
    void release();
    int w_ = 0, h_ = 0;
    unsigned msFbo_ = 0, msColor_ = 0, msDepth_ = 0;
    unsigned fbo_ = 0, tex_ = 0;
    int prevFbo_ = 0;
};

} // namespace mazz
