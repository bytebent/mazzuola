#include "core/selection.hpp"
#include <algorithm>

namespace mazz {

bool Selection::empty() const {
    switch (mode) {
    case SelectMode::Vert: return verts.empty();
    case SelectMode::Edge: return edges.empty();
    default: return faces.empty();
    }
}

std::set<int> wholePieces(const Mesh& m, const std::set<int>& faces) {
    std::vector<int> piece;
    pieceIds(m, piece);
    std::set<int> wanted;
    for (int f : faces)
        if (f >= 0 && f < (int)piece.size()) wanted.insert(piece[f]);
    std::set<int> out;
    for (int i = 0; i < (int)piece.size(); ++i)
        if (wanted.count(piece[i])) out.insert(i);
    return out;
}

std::set<int> selectedVerts(const Mesh& m, const Selection& s) {
    std::set<int> out;
    switch (s.mode) {
    case SelectMode::Vert:
        for (int v : s.verts)
            if (v >= 0 && v < (int)m.verts.size()) out.insert(v);
        break;
    case SelectMode::Edge:
        for (const Edge& e : s.edges) {
            if (e.first >= 0 && e.second < (int)m.verts.size()) {
                out.insert(e.first);
                out.insert(e.second);
            }
        }
        break;
    default:
        for (int f : s.faces)
            if (f >= 0 && f < (int)m.faces.size())
                for (int k = 0; k < m.faces[f].n; ++k) out.insert(m.faces[f].v[k]);
        break;
    }
    return out;
}

bool selectionBounds(const Mesh& m, const Selection& s, Vec3& mn, Vec3& mx) {
    const std::set<int> vs = selectedVerts(m, s);
    if (vs.empty()) return false;
    mn = mx = m.verts[*vs.begin()];
    for (int v : vs)
        for (int a = 0; a < 3; ++a) {
            mn[a] = std::min(mn[a], m.verts[v][a]);
            mx[a] = std::max(mx[a], m.verts[v][a]);
        }
    return true;
}

void dropMissing(const Mesh& m, Selection& s) {
    const int nv = (int)m.verts.size(), nf = (int)m.faces.size();
    for (auto it = s.verts.begin(); it != s.verts.end();) it = (*it >= nv) ? s.verts.erase(it) : std::next(it);
    for (auto it = s.edges.begin(); it != s.edges.end();) it = (it->second >= nv) ? s.edges.erase(it) : std::next(it);
    for (auto it = s.faces.begin(); it != s.faces.end();) it = (*it >= nf) ? s.faces.erase(it) : std::next(it);
}

} // namespace mazz
