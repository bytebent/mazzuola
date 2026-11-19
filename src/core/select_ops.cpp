#include "core/select_ops.hpp"
#include "core/pick.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace mazz {

namespace {

std::set<Edge> faceEdges(const Face& f) {
    std::set<Edge> out;
    for (int k = 0; k < f.n; ++k) out.insert(makeEdge(f.v[k], f.v[(k + 1) % f.n]));
    return out;
}

std::set<Edge> allEdges(const Mesh& m) {
    std::set<Edge> out;
    for (const Face& f : m.faces)
        for (int k = 0; k < f.n; ++k) out.insert(makeEdge(f.v[k], f.v[(k + 1) % f.n]));
    return out;
}

std::set<int> facePoints(const Mesh& m, const std::set<int>& faces) {
    std::set<int> out;
    for (int f : faces)
        if (f >= 0 && f < (int)m.faces.size())
            for (int k = 0; k < m.faces[f].n; ++k) out.insert(m.faces[f].v[k]);
    return out;
}

// Faces using each edge.
std::map<Edge, std::vector<int>> edgeFaces(const Mesh& m) {
    std::map<Edge, std::vector<int>> out;
    for (int i = 0; i < (int)m.faces.size(); ++i)
        for (const Edge& e : faceEdges(m.faces[i])) out[e].push_back(i);
    return out;
}

// Edges at each point.
std::map<int, std::set<Edge>> pointEdges(const Mesh& m) {
    std::map<int, std::set<Edge>> out;
    for (const Edge& e : allEdges(m)) {
        out[e.first].insert(e);
        out[e.second].insert(e);
    }
    return out;
}

double segDist(double ax, double ay, double bx, double by, double x, double y) {
    const double dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy;
    double t = len2 > 0 ? ((x - ax) * dx + (y - ay) * dy) / len2 : 0;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(ax + t * dx - x, ay + t * dy - y);
}

// How near a world point is to the viewer (larger = nearer).
double nearness(const ViewGeom& g, const Vec3& p) {
    if (g.is3D()) return -length(p - g.cam.eye());
    return dot(p, normalize(g.axes().forward()) * -1.0);
}

bool insidePoly(const std::vector<std::pair<double, double>>& poly, double x, double y) {
    bool in = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const auto& a = poly[i];
        const auto& b = poly[j];
        if ((a.second > y) != (b.second > y) && x < (b.first - a.first) * (y - a.second) / (b.second - a.second) + a.first)
            in = !in;
    }
    return in;
}

// Visible from the 3D camera: nothing between the eye and `p` except faces touching it.
bool visibleFrom(const Mesh& m, const ViewGeom& g, const Vec3& p, const std::set<int>& ownFaces) {
    const Vec3 eye = g.cam.eye();
    const Vec3 d = p - eye;
    const double dist = length(d);
    if (dist < 1e-9) return true;
    const Vec3 dir = d / dist;
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        if (ownFaces.count(fi)) continue;
        const Face& f = m.faces[fi];
        for (int k = 1; k + 1 < f.n; ++k) {
            double t = 0;
            if (rayTriangle(eye, dir, m.verts[f.v[0]], m.verts[f.v[k]], m.verts[f.v[k + 1]], t) && t < dist * (1 - 1e-6))
                return false;
        }
    }
    return true;
}

std::set<int> facesWithPoint(const Mesh& m, int point) {
    std::set<int> out;
    for (int i = 0; i < (int)m.faces.size(); ++i)
        for (int k = 0; k < m.faces[i].n; ++k)
            if (m.faces[i].v[k] == point) out.insert(i);
    return out;
}

} // namespace

bool pointVisible(const Mesh& m, const ViewGeom& g, int point) {
    if (!g.is3D()) return true;
    return visibleFrom(m, g, m.verts[point], facesWithPoint(m, point));
}

Selection convertSelection(const Mesh& m, const Selection& s, SelectMode to) {
    if (s.mode == to) return s;
    Selection out;
    out.mode = to;
    // Everything goes through points, except faces <-> pieces.
    std::set<int> pts;
    std::set<Edge> edges;
    std::set<int> faces;
    switch (s.mode) {
    case SelectMode::Vert: pts = s.verts; break;
    case SelectMode::Edge:
        edges = s.edges;
        for (const Edge& e : edges) {
            pts.insert(e.first);
            pts.insert(e.second);
        }
        break;
    case SelectMode::Face:
    case SelectMode::Piece:
        faces = s.faces;
        pts = facePoints(m, faces);
        for (int f : faces)
            if (f >= 0 && f < (int)m.faces.size())
                for (const Edge& e : faceEdges(m.faces[f])) edges.insert(e);
        break;
    }
    switch (to) {
    case SelectMode::Vert: out.verts = pts; break;
    case SelectMode::Edge:
        if (s.mode == SelectMode::Vert) {
            for (const Edge& e : allEdges(m))
                if (pts.count(e.first) && pts.count(e.second)) out.edges.insert(e);
        } else {
            out.edges = edges;
        }
        break;
    case SelectMode::Face:
    case SelectMode::Piece: {
        if (s.mode == SelectMode::Face || s.mode == SelectMode::Piece) {
            out.faces = faces;
        } else {
            for (int i = 0; i < (int)m.faces.size(); ++i) {
                bool all = true;
                if (s.mode == SelectMode::Vert) {
                    for (int k = 0; k < m.faces[i].n && all; ++k) all = pts.count(m.faces[i].v[k]) > 0;
                } else {
                    for (const Edge& e : faceEdges(m.faces[i])) all = all && edges.count(e) > 0;
                }
                if (all) out.faces.insert(i);
            }
        }
        if (to == SelectMode::Piece) out.faces = wholePieces(m, out.faces);
        break;
    }
    }
    return out;
}

std::set<int> pointsAt(const Mesh& m, const ViewGeom& g, double sx, double sy, double tolPx) {
    // The nearest point on screen within the tolerance...
    std::set<int> used;
    for (const Face& f : m.faces)
        for (int k = 0; k < f.n; ++k) used.insert(f.v[k]);
    int best = -1;
    double bestD = tolPx, bx = 0, by = 0;
    for (int v : used) {
        double x, y;
        if (!g.project(m.verts[v], x, y)) continue;
        const double d = std::hypot(x - sx, y - sy);
        if (d > tolPx) continue;
        if (g.is3D() && !pointVisible(m, g, v)) continue;
        // Ties (stacked points) go to the one nearest the viewer.
        if (best < 0 || d < bestD - 1e-9 ||
            (std::fabs(d - bestD) <= 1e-9 && nearness(g, m.verts[v]) > nearness(g, m.verts[best]))) {
            best = v;
            bestD = d;
            bx = x;
            by = y;
        }
    }
    std::set<int> out;
    if (best < 0) return out;
    if (g.is3D()) {
        out.insert(best);
        return out;
    }
    // ...and in 2D every point stacked on the same spot.
    for (int v : used) {
        double x, y;
        g.project(m.verts[v], x, y);
        if (std::hypot(x - bx, y - by) < 0.5) out.insert(v);
    }
    return out;
}

std::vector<Edge> edgesAt(const Mesh& m, const ViewGeom& g, double sx, double sy, double tolPx) {
    std::vector<std::pair<double, Edge>> hits;
    for (const Edge& e : allEdges(m)) {
        double ax, ay, bx, by;
        if (!g.project(m.verts[e.first], ax, ay) || !g.project(m.verts[e.second], bx, by)) continue;
        if (segDist(ax, ay, bx, by, sx, sy) > tolPx) continue;
        const Vec3 mid = (m.verts[e.first] + m.verts[e.second]) / 2;
        if (g.is3D()) {
            std::set<int> own = facesWithPoint(m, e.first);
            for (int f : facesWithPoint(m, e.second)) own.insert(f);
            if (!visibleFrom(m, g, mid, own)) continue;
        }
        hits.push_back({-nearness(g, mid), e});
    }
    std::sort(hits.begin(), hits.end());
    std::vector<Edge> out;
    for (const auto& h : hits) out.push_back(h.second);
    return out;
}

std::vector<int> facesAt(const Mesh& m, const ViewGeom& g, double sx, double sy) {
    std::vector<std::pair<double, int>> hits;
    if (g.is3D()) {
        Vec3 o, d;
        g.ray(sx, sy, o, d);
        for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
            const Face& f = m.faces[fi];
            for (int k = 1; k + 1 < f.n; ++k) {
                double t = 0;
                if (rayTriangle(o, d, m.verts[f.v[0]], m.verts[f.v[k]], m.verts[f.v[k + 1]], t)) {
                    hits.push_back({t, fi});
                    break;
                }
            }
        }
    } else {
        for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
            const Face& f = m.faces[fi];
            std::vector<std::pair<double, double>> poly;
            for (int k = 0; k < f.n; ++k) {
                double x, y;
                g.project(m.verts[f.v[k]], x, y);
                poly.push_back({x, y});
            }
            if (!insidePoly(poly, sx, sy)) continue;
            // Nearest to the viewer: its nearest corner, then its centre.
            double near = -std::numeric_limits<double>::max();
            for (int k = 0; k < f.n; ++k) near = std::max(near, nearness(g, m.verts[f.v[k]]));
            hits.push_back({-(near * 1e3 + nearness(g, faceCentre(m, fi))), fi});
        }
    }
    std::sort(hits.begin(), hits.end());
    std::vector<int> out;
    for (const auto& h : hits) out.push_back(h.second);
    return out;
}

Selection inRect(const Mesh& m, const ViewGeom& g, double x0, double y0, double x1, double y1, SelectMode mode) {
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    Selection out;
    out.mode = mode;
    if (mode == SelectMode::Piece) {
        out.faces = facesOfPieces(m, piecesInRect(m, g, x0, y0, x1, y1));
        return out;
    }
    std::map<int, bool> inside; // point -> inside the rectangle (and visible in 3D)
    auto isIn = [&](int v) {
        auto it = inside.find(v);
        if (it != inside.end()) return it->second;
        double x, y;
        bool ok = g.project(m.verts[v], x, y) && x >= x0 && x <= x1 && y >= y0 && y <= y1;
        if (ok && g.is3D()) ok = pointVisible(m, g, v);
        return inside[v] = ok;
    };
    if (mode == SelectMode::Vert) {
        for (const Face& f : m.faces)
            for (int k = 0; k < f.n; ++k)
                if (isIn(f.v[k])) out.verts.insert(f.v[k]);
    } else if (mode == SelectMode::Edge) {
        for (const Edge& e : allEdges(m))
            if (isIn(e.first) && isIn(e.second)) out.edges.insert(e);
    } else {
        for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
            const Face& f = m.faces[fi];
            bool all = true;
            for (int k = 0; k < f.n && all; ++k) all = isIn(f.v[k]);
            // In 3D only faces turned toward the camera count as seen.
            if (all && g.is3D()) all = dot(faceNormal(m, fi), g.cam.eye() - faceCentre(m, fi)) > 0;
            if (all) out.faces.insert(fi);
        }
    }
    return out;
}

Selection growSelection(const Mesh& m, const Selection& s) {
    Selection out = s;
    switch (s.mode) {
    case SelectMode::Vert:
        for (const Edge& e : allEdges(m)) {
            if (s.verts.count(e.first)) out.verts.insert(e.second);
            if (s.verts.count(e.second)) out.verts.insert(e.first);
        }
        break;
    case SelectMode::Edge: {
        std::set<int> pts;
        for (const Edge& e : s.edges) {
            pts.insert(e.first);
            pts.insert(e.second);
        }
        for (const Edge& e : allEdges(m))
            if (pts.count(e.first) || pts.count(e.second)) out.edges.insert(e);
        break;
    }
    case SelectMode::Face: {
        const std::set<int> pts = facePoints(m, s.faces);
        for (int i = 0; i < (int)m.faces.size(); ++i)
            for (int k = 0; k < m.faces[i].n; ++k)
                if (pts.count(m.faces[i].v[k])) out.faces.insert(i);
        break;
    }
    case SelectMode::Piece: break;
    }
    return out;
}

Selection shrinkSelection(const Mesh& m, const Selection& s) {
    Selection out = s;
    switch (s.mode) {
    case SelectMode::Vert:
        // Drop points with a neighbour that isn't selected.
        for (const Edge& e : allEdges(m)) {
            const bool a = s.verts.count(e.first), b = s.verts.count(e.second);
            if (a && !b) out.verts.erase(e.first);
            if (b && !a) out.verts.erase(e.second);
        }
        break;
    case SelectMode::Edge: {
        // Drop edges touching an edge that isn't selected.
        std::set<int> border;
        for (const Edge& e : allEdges(m))
            if (!s.edges.count(e)) {
                border.insert(e.first);
                border.insert(e.second);
            }
        for (const Edge& e : s.edges)
            if (border.count(e.first) || border.count(e.second)) out.edges.erase(e);
        break;
    }
    case SelectMode::Face: {
        // Drop faces sharing a point with a face that isn't selected.
        std::set<int> border;
        for (int i = 0; i < (int)m.faces.size(); ++i)
            if (!s.faces.count(i))
                for (int k = 0; k < m.faces[i].n; ++k) border.insert(m.faces[i].v[k]);
        for (int f : s.faces)
            for (int k = 0; k < m.faces[f].n; ++k)
                if (border.count(m.faces[f].v[k])) {
                    out.faces.erase(f);
                    break;
                }
        break;
    }
    case SelectMode::Piece: break;
    }
    return out;
}

std::set<Edge> edgeLoop(const Mesh& m, Edge start) {
    const auto ef = edgeFaces(m);
    const auto pe = pointEdges(m);
    std::set<Edge> loop;
    if (!ef.count(start)) return loop;
    loop.insert(start);
    // Walk from one end, then from the other.
    for (int dir = 0; dir < 2; ++dir) {
        Edge e = start;
        int at = dir == 0 ? start.second : start.first;
        for (int guard = 0; guard < 100000; ++guard) {
            auto pit = pe.find(at);
            if (pit == pe.end() || pit->second.size() != 4) break; // only through four-sided corners
            // The edges sharing a face with `e` at this point are its neighbours; the one
            // left over goes straight on.
            std::set<Edge> beside;
            auto fit = ef.find(e);
            if (fit == ef.end() || fit->second.size() != 2) break;
            for (int f : fit->second)
                for (const Edge& fe : faceEdges(m.faces[f]))
                    if (fe != e && (fe.first == at || fe.second == at)) beside.insert(fe);
            Edge next{-1, -1};
            int count = 0;
            for (const Edge& c : pit->second)
                if (c != e && !beside.count(c)) {
                    next = c;
                    ++count;
                }
            if (count != 1 || loop.count(next)) break;
            loop.insert(next);
            at = next.first == at ? next.second : next.first;
            e = next;
        }
    }
    return loop;
}

std::set<Edge> edgeRing(const Mesh& m, Edge start) {
    const auto ef = edgeFaces(m);
    std::set<Edge> ring;
    if (!ef.count(start)) return ring;
    ring.insert(start);
    std::vector<std::pair<Edge, int>> todo; // edge, face it came from
    todo.push_back({start, -1});
    while (!todo.empty()) {
        const auto [e, from] = todo.back();
        todo.pop_back();
        for (int f : ef.at(e)) {
            if (f == from || m.faces[f].n != 4) continue;
            // The opposite side of the quad: the edge touching neither end of `e`.
            for (const Edge& o : faceEdges(m.faces[f]))
                if (o.first != e.first && o.first != e.second && o.second != e.first && o.second != e.second &&
                    !ring.count(o)) {
                    ring.insert(o);
                    todo.push_back({o, f});
                }
        }
    }
    return ring;
}

std::set<int> facesToDelete(const Mesh& m, const Selection& s) {
    std::set<int> out;
    switch (s.mode) {
    case SelectMode::Face:
    case SelectMode::Piece: return s.faces;
    case SelectMode::Vert:
        for (int i = 0; i < (int)m.faces.size(); ++i)
            for (int k = 0; k < m.faces[i].n; ++k)
                if (s.verts.count(m.faces[i].v[k])) {
                    out.insert(i);
                    break;
                }
        break;
    case SelectMode::Edge:
        for (int i = 0; i < (int)m.faces.size(); ++i)
            for (const Edge& e : faceEdges(m.faces[i]))
                if (s.edges.count(e)) {
                    out.insert(i);
                    break;
                }
        break;
    }
    return out;
}

} // namespace mazz
