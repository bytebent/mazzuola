#include "core/pick.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace mazz {

namespace {
struct P2 {
    double x, y;
};

bool insidePolygon(const std::vector<P2>& poly, double x, double y) {
    bool in = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const P2& a = poly[i];
        const P2& b = poly[j];
        if ((a.y > y) != (b.y > y) && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x) in = !in;
    }
    return in;
}

double segDist(const P2& a, const P2& b, double x, double y) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double len2 = dx * dx + dy * dy;
    double t = len2 > 0 ? ((x - a.x) * dx + (y - a.y) * dy) / len2 : 0;
    t = std::clamp(t, 0.0, 1.0);
    const double px = a.x + t * dx - x, py = a.y + t * dy - y;
    return std::sqrt(px * px + py * py);
}
} // namespace

std::vector<int> piecesAt(const Mesh& m, const ViewGeom& g, double sx, double sy, double tolPx) {
    std::vector<int> piece;
    pieceIds(m, piece);
    if (g.is3D()) {
        Vec3 o, d;
        g.ray(sx, sy, o, d);
        double best = std::numeric_limits<double>::max();
        int hit = -1;
        for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
            const Face& f = m.faces[fi];
            for (int k = 1; k + 1 < f.n; ++k) {
                double t = 0;
                if (rayTriangle(o, d, m.verts[f.v[0]], m.verts[f.v[k]], m.verts[f.v[k + 1]], t) && t < best) {
                    best = t;
                    hit = fi;
                }
            }
        }
        return hit < 0 ? std::vector<int>{} : std::vector<int>{piece[hit]};
    }
    // 2D: how near each piece comes to the viewer, among the faces under the point.
    const Vec3 toViewer = normalize(g.axes().forward()) * -1.0;
    std::map<int, double> nearest; // piece -> depth toward the viewer (larger = nearer)
    std::vector<P2> poly;
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Face& f = m.faces[fi];
        poly.clear();
        for (int k = 0; k < f.n; ++k) {
            double x, y;
            g.project(m.verts[f.v[k]], x, y);
            poly.push_back({x, y});
        }
        bool hit = insidePolygon(poly, sx, sy);
        for (int k = 0; k < f.n && !hit; ++k) hit = segDist(poly[k], poly[(k + 1) % f.n], sx, sy) <= tolPx;
        if (!hit) continue;
        double depth = -std::numeric_limits<double>::max();
        for (int k = 0; k < f.n; ++k) depth = std::max(depth, dot(m.verts[f.v[k]], toViewer));
        auto it = nearest.find(piece[fi]);
        if (it == nearest.end() || depth > it->second) nearest[piece[fi]] = depth;
    }
    std::vector<std::pair<double, int>> order;
    for (const auto& [p, depth] : nearest) order.push_back({-depth, p});
    std::sort(order.begin(), order.end());
    std::vector<int> out;
    for (const auto& o : order) out.push_back(o.second);
    return out;
}

std::set<int> piecesInRect(const Mesh& m, const ViewGeom& g, double x0, double y0, double x1, double y1) {
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    std::vector<int> piece;
    const int count = pieceIds(m, piece);
    std::vector<char> ok(count, 1), any(count, 0);
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Face& f = m.faces[fi];
        any[piece[fi]] = 1;
        for (int k = 0; k < f.n; ++k) {
            double x, y;
            if (!g.project(m.verts[f.v[k]], x, y) || x < x0 || x > x1 || y < y0 || y > y1) ok[piece[fi]] = 0;
        }
    }
    std::set<int> out;
    for (int p = 0; p < count; ++p)
        if (ok[p] && any[p]) out.insert(p);
    return out;
}

std::set<int> facesOfPieces(const Mesh& m, const std::set<int>& pieces) {
    std::vector<int> piece;
    pieceIds(m, piece);
    std::set<int> out;
    for (int fi = 0; fi < (int)piece.size(); ++fi)
        if (pieces.count(piece[fi])) out.insert(fi);
    return out;
}

} // namespace mazz
