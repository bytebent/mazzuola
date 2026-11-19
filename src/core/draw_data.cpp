#include "core/draw_data.hpp"
#include <algorithm>
#include <map>
#include <set>

namespace mazz {

namespace {
DrawVertex dv(const Vec3& p, const Vec3& n) {
    return {(float)p.x, (float)p.y, (float)p.z, (float)n.x, (float)n.y, (float)n.z};
}
} // namespace

void buildDrawData(const Mesh& m, DrawData& out) {
    out.tris.clear();
    out.edges.clear();
    out.edgeList.clear();
    out.faceFirst.assign(m.faces.size(), 0);
    out.faceCount.assign(m.faces.size(), 0);
    std::set<Edge> seen;
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Face& f = m.faces[fi];
        const Vec3 n = faceNormal(m, fi);
        out.faceFirst[fi] = (int)out.tris.size();
        auto tri = [&](int a, int b, int c) {
            out.tris.push_back(dv(m.verts[f.v[a]], n));
            out.tris.push_back(dv(m.verts[f.v[b]], n));
            out.tris.push_back(dv(m.verts[f.v[c]], n));
        };
        if (f.n == 3) tri(0, 1, 2);
        else if (!f.diag) { tri(0, 1, 2); tri(0, 2, 3); }
        else { tri(0, 1, 3); tri(1, 2, 3); }
        out.faceCount[fi] = (int)out.tris.size() - out.faceFirst[fi];
        for (int k = 0; k < f.n; ++k) {
            const Edge e = makeEdge(f.v[k], f.v[(k + 1) % f.n]);
            if (!seen.insert(e).second) continue;
            out.edgeList.push_back(e);
            out.edges.push_back(dv(m.verts[e.first], {}));
            out.edges.push_back(dv(m.verts[e.second], {}));
        }
    }
}

std::vector<unsigned> selectedTriIndices(const DrawData& d, const Selection& s) {
    std::vector<unsigned> out;
    if (s.mode != SelectMode::Face && s.mode != SelectMode::Piece) return out;
    for (int f : s.faces) {
        if (f < 0 || f >= (int)d.faceFirst.size()) continue;
        for (int k = 0; k < d.faceCount[f]; ++k) out.push_back((unsigned)(d.faceFirst[f] + k));
    }
    return out;
}

std::vector<unsigned> selectedEdgeIndices(const Mesh& m, const DrawData& d, const Selection& s) {
    std::set<Edge> want;
    if (s.mode == SelectMode::Edge) {
        want = s.edges;
    } else if (s.mode == SelectMode::Vert) {
        for (const Edge& e : d.edgeList)
            if (s.verts.count(e.first) && s.verts.count(e.second)) want.insert(e);
    } else {
        for (int fi : s.faces) {
            if (fi < 0 || fi >= (int)m.faces.size()) continue;
            const Face& f = m.faces[fi];
            for (int k = 0; k < f.n; ++k) want.insert(makeEdge(f.v[k], f.v[(k + 1) % f.n]));
        }
    }
    std::vector<unsigned> out;
    for (int i = 0; i < (int)d.edgeList.size(); ++i)
        if (want.count(d.edgeList[i])) {
            out.push_back((unsigned)(2 * i));
            out.push_back((unsigned)(2 * i + 1));
        }
    return out;
}

} // namespace mazz
