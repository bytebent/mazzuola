#pragma once
// The model on the GPU (ARCHITECTURE §9): its triangles and edges are uploaded once per
// geometry change and shared by every view; a selection change only re-uploads the
// small index lists of the selected triangles and edges.
#include "core/draw_data.hpp"
#include "core/math.hpp"

namespace mazz {

class ModelRenderer {
public:
    ModelRenderer();
    ~ModelRenderer();
    ModelRenderer(const ModelRenderer&) = delete;
    ModelRenderer& operator=(const ModelRenderer&) = delete;

    bool ok() const { return program_ != 0; }
    void setGeometry(const DrawData& d);
    void setSelection(const std::vector<unsigned>& tris, const std::vector<unsigned>& edges);

    // 3D: shaded faces, their edges, the selection tinted. `toLight` points from the
    // model toward the light. 2D: edges only, the selection's edges highlighted.
    void draw3D(const Mat4& viewProj, const Vec3& toLight);
    void draw2D(const Mat4& viewProj);

private:
    unsigned program_ = 0;
    int locMvp_ = -1, locColor_ = -1, locLight_ = -1, locLit_ = -1;
    unsigned triVao_ = 0, triVbo_ = 0, edgeVao_ = 0, edgeVbo_ = 0;
    unsigned selTriEbo_ = 0, selEdgeEbo_ = 0;
    int triCount_ = 0, edgeCount_ = 0, selTriCount_ = 0, selEdgeCount_ = 0;
    void use(const Mat4& mvp, float r, float g, float b, float a, bool lit, const Vec3& toLight = {});
};

} // namespace mazz
