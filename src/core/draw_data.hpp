#pragma once
// What the views draw for the model, built once per geometry change and shared by all
// views (ARCHITECTURE §9): triangles with flat normals, and each edge once. Pure data;
// the render layer uploads it to the GPU.
#include "core/mesh.hpp"
#include "core/selection.hpp"
#include <vector>

namespace mazz {

struct DrawVertex {
    float x, y, z;    // position
    float nx, ny, nz; // face normal
};

struct DrawData {
    std::vector<DrawVertex> tris;     // 3 per triangle
    std::vector<int> faceFirst;       // per face: index of its first vertex in `tris`
    std::vector<int> faceCount;       // per face: number of vertices (3 or 6)
    std::vector<DrawVertex> edges;    // 2 per edge (normal unused)
    std::vector<Edge> edgeList;       // the same edges as point pairs
};

void buildDrawData(const Mesh& m, DrawData& out);
// Vertex indices into `tris` of the selected faces (Face and Piece modes).
std::vector<unsigned> selectedTriIndices(const DrawData& d, const Selection& s);
// Vertex indices into `edges` of the selection's edges (every mode: the edges of the
// selected faces, the selected edges, or the edges between two selected points).
std::vector<unsigned> selectedEdgeIndices(const Mesh& m, const DrawData& d, const Selection& s);

} // namespace mazz
