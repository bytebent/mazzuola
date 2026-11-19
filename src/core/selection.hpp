#pragma once
// What is selected, together with the selection mode (ARCHITECTURE §5): the mode lives
// in the selection, so undo/redo always bring back a selection that matches its mode.
#include "core/mesh.hpp"
#include <set>
#include <utility>

namespace mazz {

enum class SelectMode { Vert, Edge, Face, Piece };

using Edge = std::pair<int, int>; // point indices, first < second
inline Edge makeEdge(int a, int b) { return a < b ? Edge{a, b} : Edge{b, a}; }

struct Selection {
    SelectMode mode = SelectMode::Piece;
    std::set<int> verts; // Vert mode
    std::set<Edge> edges; // Edge mode
    std::set<int> faces; // Face mode, and Piece mode (all faces of the chosen pieces)

    bool empty() const;
    void clear() { verts.clear(); edges.clear(); faces.clear(); }
    bool operator==(const Selection& o) const {
        return mode == o.mode && verts == o.verts && edges == o.edges && faces == o.faces;
    }
    bool operator!=(const Selection& o) const { return !(*this == o); }
};

// All faces of the pieces that contain any of `faces`.
std::set<int> wholePieces(const Mesh& m, const std::set<int>& faces);
// The points the selection covers (for moving, bounds...).
std::set<int> selectedVerts(const Mesh& m, const Selection& s);
// Bounds of the selected points (false if nothing is selected).
bool selectionBounds(const Mesh& m, const Selection& s, Vec3& mn, Vec3& mx);
// Drops entries that no longer exist in `m` (after an undo that removed faces, say).
void dropMissing(const Mesh& m, Selection& s);

} // namespace mazz
