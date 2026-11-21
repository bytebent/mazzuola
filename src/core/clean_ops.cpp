#include "core/clean_ops.hpp"
#include "core/draw_ops.hpp"
#include "core/model_ops.hpp"
#include "core/numbers.hpp"
#include "core/ops.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <vector>

namespace mazz {

namespace {

constexpr double kStraightCm = 2e-3; // a point this near the line through its neighbours is on it
constexpr double kMinArea = 1e-3;    // cm²

std::vector<int> loopOf(const Face& f) { return std::vector<int>(f.v, f.v + f.n); }

Face faceFrom(const Face& like, const std::vector<int>& vs) {
    Face f;
    f.tex = like.tex;
    f.group = like.group;
    f.sg = like.sg;
    f.twoSided = like.twoSided;
    f.n = (int)vs.size();
    for (int k = 0; k < f.n; ++k) f.v[k] = vs[k];
    return f;
}

double area(const Mesh& m, const std::vector<int>& p) {
    Vec3 s{0, 0, 0};
    for (size_t k = 1; k + 1 < p.size(); ++k) s = s + cross(m.verts[p[k]] - m.verts[p[0]], m.verts[p[k + 1]] - m.verts[p[0]]);
    return length(s) / 2;
}

struct UnionFind {
    std::map<int, int> parent;
    int find(int x) {
        auto it = parent.find(x);
        if (it == parent.end()) return parent[x] = x;
        if (it->second == x) return x;
        return it->second = find(it->second);
    }
    void join(int a, int b) {
        a = find(a), b = find(b);
        if (a != b) parent[std::max(a, b)] = std::min(a, b);
    }
};

// After points were made one: faces lose repeated corners; faces left without area,
// with a corner twice, or matched by their exact reverse (or a duplicate) go. Unused
// points go too (indices change).
void tidy(MeshEdit& e) {
    const Mesh& m = e.mesh();
    std::set<int> gone;
    std::map<std::vector<int>, int> seen; // canonical cycle -> face
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Face f = m.faces[fi];
        std::vector<int> l;
        for (int k = 0; k < f.n; ++k)
            if (l.empty() || l.back() != f.v[k]) l.push_back(f.v[k]);
        while (l.size() > 1 && l.front() == l.back()) l.pop_back();
        std::set<int> distinct(l.begin(), l.end());
        if (l.size() < 3 || distinct.size() != l.size() || area(m, l) < kMinArea) {
            gone.insert(fi);
            continue;
        }
        if ((int)l.size() != f.n) {
            Face g = faceFrom(f, l);
            e.setFace(fi, g);
        }
        // Same corners as another face (either way round): both go if reversed, one if the same.
        std::vector<int> c = l;
        std::rotate(c.begin(), std::min_element(c.begin(), c.end()), c.end());
        std::vector<int> r(l.rbegin(), l.rend());
        std::rotate(r.begin(), std::min_element(r.begin(), r.end()), r.end());
        auto it = seen.find(r);
        if (it != seen.end() && !gone.count(it->second)) {
            gone.insert(fi);
            gone.insert(it->second);
            continue;
        }
        if (seen.count(c)) {
            gone.insert(fi);
            continue;
        }
        seen[c] = fi;
    }
    e.deleteFaces(gone); // also drops unused points
}

// Points merged without tidying (several groups, then one tidy).
void mergeRaw(MeshEdit& e, const std::set<int>& verts, const Vec3& at, int keep) {
    if (verts.empty()) return;
    if (!verts.count(keep)) keep = *verts.begin();
    e.setVert(keep, at);
    const Mesh& m = e.mesh();
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        Face f = m.faces[fi];
        bool changed = false;
        for (int k = 0; k < f.n; ++k)
            if (f.v[k] != keep && verts.count(f.v[k])) f.v[k] = keep, changed = true;
        if (changed) e.setFace(fi, f);
    }
}

Vec3 centreOf(const Mesh& m, const std::set<int>& verts) {
    Vec3 c{0, 0, 0};
    for (int v : verts) c = c + m.verts[v];
    return c * (1.0 / verts.size());
}

// Faces using a point; its neighbour points (by edges).
std::vector<int> facesAround(const Mesh& m, int v) {
    std::vector<int> out;
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Face& f = m.faces[fi];
        for (int k = 0; k < f.n; ++k)
            if (f.v[k] == v) {
                out.push_back(fi);
                break;
            }
    }
    return out;
}

std::set<int> neighbours(const Mesh& m, int v) {
    std::set<int> n;
    for (const Face& f : m.faces)
        for (int k = 0; k < f.n; ++k)
            if (f.v[k] == v) n.insert(f.v[(k + 1) % f.n]), n.insert(f.v[(k + f.n - 1) % f.n]);
    return n;
}

// The single outline loop of a set of faces (empty: not one loop).
std::vector<int> outline(const Mesh& m, const std::set<int>& region) {
    std::set<std::pair<int, int>> all;
    for (int fi : region) {
        const Face& f = m.faces[fi];
        for (int k = 0; k < f.n; ++k) all.insert({f.v[k], f.v[(k + 1) % f.n]});
    }
    std::map<int, int> next;
    for (const auto& d : all)
        if (!all.count({d.second, d.first})) {
            if (next.count(d.first)) return {};
            next[d.first] = d.second;
        }
    if (next.empty()) return {};
    std::vector<int> loop;
    int v = next.begin()->first;
    do {
        loop.push_back(v);
        auto it = next.find(v);
        if (it == next.end() || loop.size() > next.size()) return {};
        v = it->second;
    } while (v != loop.front());
    if (loop.size() != next.size()) return {};
    return loop;
}

// How a region of faces would be replaced by one outline split into 3- and 4-corner
// faces (empty: it can't, e.g. too bent). Changes nothing.
std::vector<std::vector<int>> planJoin(const Mesh& m, const std::set<int>& region, const std::vector<int>& loop) {
    Vec3 n{0, 0, 0};
    for (int fi : region) n = n + faceNormal(m, fi);
    if (length(n) < 1e-6) return {};
    n = n * (1.0 / length(n));
    auto pieces = splitPolygon(m, loop, n);
    for (const auto& p : pieces)
        if (area(m, p) < kMinArea) return {};
    return pieces;
}

// Puts the planned pieces in the region's face slots (more pieces: new faces; fewer: the
// slots left over go into `gone`).
void applyJoin(MeshEdit& e, const std::set<int>& region, const std::vector<std::vector<int>>& pieces,
               std::set<int>& gone, std::vector<int>* made = nullptr) {
    const Face like = e.mesh().faces[*region.begin()];
    std::vector<int> slots(region.begin(), region.end());
    for (size_t i = 0; i < pieces.size(); ++i) {
        const Face f = faceFrom(like, pieces[i]);
        if (i < slots.size()) {
            e.setFace(slots[i], f);
            if (made) made->push_back(slots[i]);
        } else {
            const int idx = e.addFace(f);
            if (made) made->push_back(idx);
        }
    }
    for (size_t i = pieces.size(); i < slots.size(); ++i) gone.insert(slots[i]);
}

// Replaces a region of faces by one outline split into 3- and 4-corner faces.
bool rejoin(MeshEdit& e, const std::set<int>& region, const std::vector<int>& loop, std::set<int>& gone,
            std::vector<int>* made = nullptr) {
    const auto pieces = planJoin(e.mesh(), region, loop);
    if (pieces.empty()) return false;
    applyJoin(e, region, pieces, gone, made);
    return true;
}

// Point p lies on the straight line from a to b, between them.
bool straightBetween(const Mesh& m, int p, int a, int b) {
    const Vec3 A = m.verts[a], B = m.verts[b], P = m.verts[p];
    const double len = length(B - A);
    if (len < 1e-9) return false;
    const double along = dot(P - A, B - A) / len;
    return along > 0 && along < len && length(cross(P - A, B - A)) / len <= kStraightCm;
}

// Takes point p, lying straight between its outline neighbours a and b, out of the
// faces round it: the faces on each side of the line a-p-b join without it (each side
// gets its old shape back). False, with nothing changed, when it can't. `kept` (face
// indices) and `vmap` (old point -> new point) follow the renumbering.
bool dropStraightPoint(MeshEdit& e, int p, int a, int b, std::set<int>& kept, std::vector<int>& vmap) {
    const Mesh& m = e.mesh();
    std::map<int, int> byPrev, nextOf; // corner before p -> face; face -> corner after p
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Face& f = m.faces[fi];
        for (int k = 0; k < f.n; ++k)
            if (f.v[k] == p) {
                const int prev = f.v[(k + f.n - 1) % f.n];
                if (byPrev.count(prev)) return false;
                byPrev[prev] = fi;
                nextOf[fi] = f.v[(k + 1) % f.n];
            }
    }
    // Round p from one end of the line to the other; empty when nothing is there (an open edge).
    auto walk = [&](int from, int to, std::vector<int>& side) {
        int x = from;
        while (side.size() <= byPrev.size()) {
            auto it = byPrev.find(x);
            if (it == byPrev.end()) return side.empty() && x == from;
            side.push_back(it->second);
            x = nextOf[it->second];
            if (x == to) return true;
        }
        return false;
    };
    std::vector<int> s1, s2;
    if (!walk(a, b, s1) || !walk(b, a, s2) || s1.size() + s2.size() != byPrev.size()) return false;
    std::vector<std::pair<std::set<int>, std::vector<std::vector<int>>>> plans;
    for (const auto* side : {&s1, &s2}) {
        if (side->empty()) continue;
        const std::set<int> region(side->begin(), side->end());
        if (region.size() != side->size()) return false;
        std::vector<int> loop = outline(m, region);
        if (loop.empty()) return false;
        loop.erase(std::remove(loop.begin(), loop.end(), p), loop.end());
        if (loop.size() < 3) return false;
        auto pieces = planJoin(m, region, loop);
        if (pieces.empty()) return false;
        plans.push_back({region, pieces});
    }
    std::set<int> gone, nowKept;
    for (const auto& [region, pieces] : plans) {
        bool allKept = true;
        for (int fi : region) allKept = allKept && kept.count(fi) > 0;
        std::vector<int> made;
        applyJoin(e, region, pieces, gone, &made);
        if (allKept) nowKept.insert(made.begin(), made.end());
    }
    for (int fi : kept)
        if (!nowKept.count(fi) && !gone.count(fi)) {
            bool inPlan = false;
            for (const auto& pl : plans) inPlan = inPlan || pl.first.count(fi) > 0;
            if (!inPlan) nowKept.insert(fi);
        }
    std::vector<int> fmap;
    e.deleteFaces(gone, &fmap, &vmap); // p is used by no face now: it goes
    kept.clear();
    for (int fi : nowKept)
        if (fmap[fi] >= 0) kept.insert(fmap[fi]);
    return true;
}

} // namespace

// ---------------------------------------------------------------- merge, collapse, weld

bool mergePoints(MeshEdit& e, const std::set<int>& verts, const Vec3& at, int keep) {
    if (verts.size() < 2) return false;
    const bool closed = isClosed(e.mesh());
    mergeRaw(e, verts, at, keep);
    tidy(e);
    return !closed || isClosed(e.mesh()); // a closed model must stay closed (caller undoes)
}

bool collapseEdges(MeshEdit& e, const std::set<Edge>& edges) {
    if (edges.empty()) return false;
    const bool closed = isClosed(e.mesh());
    UnionFind uf;
    for (const Edge& ed : edges) uf.join(ed.first, ed.second);
    std::map<int, std::set<int>> groups;
    for (const Edge& ed : edges) groups[uf.find(ed.first)].insert(ed.first), groups[uf.find(ed.first)].insert(ed.second);
    for (const auto& [root, g] : groups) mergeRaw(e, g, centreOf(e.mesh(), g), root);
    tidy(e);
    return !closed || isClosed(e.mesh());
}

int weldByDistance(MeshEdit& e, const std::set<int>& vertsIn, double dist) {
    const Mesh& m = e.mesh();
    std::vector<int> vs(vertsIn.begin(), vertsIn.end());
    if (vs.empty()) {
        vs.resize(m.verts.size());
        std::iota(vs.begin(), vs.end(), 0);
    }
    std::sort(vs.begin(), vs.end(), [&](int a, int b) { return m.verts[a].x < m.verts[b].x; });
    UnionFind uf;
    for (size_t i = 0; i < vs.size(); ++i)
        for (size_t j = i + 1; j < vs.size() && m.verts[vs[j]].x - m.verts[vs[i]].x <= dist; ++j)
            if (length(m.verts[vs[i]] - m.verts[vs[j]]) <= dist) uf.join(vs[i], vs[j]);
    std::map<int, std::set<int>> groups;
    for (int v : vs) groups[uf.find(v)].insert(v);
    int removed = 0;
    for (const auto& [root, g] : groups) {
        if (g.size() < 2) continue;
        removed += (int)g.size() - 1;
        mergeRaw(e, g, e.mesh().verts[root], root);
    }
    if (removed) tidy(e);
    return removed;
}

// ---------------------------------------------------------------- dissolve

bool dissolvePoints(MeshEdit& e, const std::set<int>& vertsIn, std::string* why) {
    auto fail = [&](const std::string& w) {
        if (why) *why = w;
        return false;
    };
    const bool closed = isClosed(e.mesh());
    if (vertsIn.empty()) return fail("select points first");
    // Followed by index: points are renumbered as others go (two points can share a spot).
    std::vector<int> todo(vertsIn.begin(), vertsIn.end());
    for (size_t t = 0; t < todo.size(); ++t) {
        const Mesh& m = e.mesh();
        const int v = todo[t];
        if (v < 0) continue;
        const std::vector<int> around = facesAround(m, v);
        if (around.empty()) continue;
        const std::set<int> nb = neighbours(m, v);
        std::set<int> gone;
        if (nb.size() == 2) {
            // On a line between two points: it just leaves its faces.
            for (int fi : around)
                if (m.faces[fi].n == 3) return fail("a point between two triangles' corners can't go (the triangle would be flat)");
            for (int fi : around) {
                std::vector<int> l;
                for (int x : loopOf(m.faces[fi]))
                    if (x != v) l.push_back(x);
                e.setFace(fi, faceFrom(m.faces[fi], l));
            }
            continue;
        }
        // The faces round it become one, without it.
        const std::set<int> region(around.begin(), around.end());
        std::vector<int> loop = outline(m, region);
        if (loop.empty()) return fail("the faces round a point don't make one outline");
        loop.erase(std::remove(loop.begin(), loop.end(), v), loop.end());
        if (loop.size() < 3) return fail("nothing would be left there");
        if (!rejoin(e, region, loop, gone)) return fail("the faces round a point are too bent to join");
        std::vector<int> vmap;
        e.deleteFaces(gone, nullptr, &vmap);
        for (size_t u = t + 1; u < todo.size(); ++u)
            if (todo[u] >= 0) todo[u] = vmap[todo[u]];
    }
    tidy(e);
    if (closed && !isClosed(e.mesh())) return fail("it would open the model");
    return true;
}

bool dissolveEdges(MeshEdit& e, const std::set<Edge>& edges, std::string* why) {
    auto fail = [&](const std::string& w) {
        if (why) *why = w;
        return false;
    };
    const bool closed = isClosed(e.mesh());
    const Mesh& m = e.mesh();
    // Faces joined across the selected edges (edges with a face on each side).
    std::map<std::pair<int, int>, int> owner;
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Face& f = m.faces[fi];
        for (int k = 0; k < f.n; ++k) owner[{f.v[k], f.v[(k + 1) % f.n]}] = fi;
    }
    UnionFind uf;
    std::set<int> ends;
    for (const Edge& ed : edges) {
        auto a = owner.find({ed.first, ed.second}), b = owner.find({ed.second, ed.first});
        if (a == owner.end() || b == owner.end()) continue;
        uf.join(a->second, b->second);
        ends.insert(ed.first);
        ends.insert(ed.second);
    }
    std::map<int, std::set<int>> regions;
    for (const auto& [f, p] : uf.parent) {
        (void)p;
        regions[uf.find(f)].insert(f);
    }
    if (regions.empty()) return fail("select edges with a face on each side");
    // Ends left with just two edges in a straight line (a loop cut's points on the sides)
    // go too: out of every face, before the faces join.
    std::set<int> straight;
    for (int v : ends) {
        std::set<int> nb = neighbours(m, v);
        for (const Edge& ed : edges)
            if (ed.first == v) nb.erase(ed.second);
            else if (ed.second == v) nb.erase(ed.first);
        if (nb.size() != 2) continue;
        const Vec3 a = m.verts[*nb.begin()], b = m.verts[*nb.rbegin()], p = m.verts[v];
        if (length(b - a) > 1e-9 && length(cross(p - a, b - a)) / length(b - a) <= kStraightCm) straight.insert(v);
    }
    // Outlines first (from the faces as they are), without the straight points.
    std::vector<std::pair<std::set<int>, std::vector<int>>> joins;
    for (const auto& [root, region] : regions) {
        (void)root;
        std::vector<int> loop = outline(m, region);
        if (loop.empty()) return fail("the faces would make a ring (an outline with a hole)");
        loop.erase(std::remove_if(loop.begin(), loop.end(), [&](int x) { return straight.count(x) > 0; }), loop.end());
        if (loop.size() < 3) return fail("nothing would be left there");
        joins.push_back({region, loop});
    }
    // Faces outside the joined ones lose the straight points too (keeping three corners).
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        if (uf.parent.count(fi)) continue;
        const Face f = m.faces[fi];
        std::vector<int> l;
        for (int x : loopOf(f))
            if (!straight.count(x)) l.push_back(x);
        if ((int)l.size() == f.n) continue;
        if (l.size() < 3) return fail("a triangle next to it would lose a corner");
        e.setFace(fi, faceFrom(f, l));
    }
    std::set<int> gone;
    for (const auto& [region, loop] : joins)
        if (!rejoin(e, region, loop, gone)) return fail("the faces are too bent to join");
    e.deleteFaces(gone);
    tidy(e);
    if (closed && !isClosed(e.mesh())) return fail("it would open the model");
    return true;
}

bool dissolveFaces(MeshEdit& e, const std::set<int>& faces, std::set<int>* out, std::string* why) {
    auto fail = [&](const std::string& w) {
        if (why) *why = w;
        return false;
    };
    const bool closed = isClosed(e.mesh());
    if (faces.empty()) return fail("select faces first");
    const Mesh& m = e.mesh();
    // Selected faces joined by shared edges.
    std::map<std::pair<int, int>, int> owner;
    for (int fi : faces) {
        const Face& f = m.faces[fi];
        for (int k = 0; k < f.n; ++k) owner[{f.v[k], f.v[(k + 1) % f.n]}] = fi;
    }
    UnionFind uf;
    for (int fi : faces) {
        uf.find(fi);
        const Face& f = m.faces[fi];
        for (int k = 0; k < f.n; ++k) {
            auto it = owner.find({f.v[(k + 1) % f.n], f.v[k]});
            if (it != owner.end()) uf.join(fi, it->second);
        }
    }
    std::map<int, std::set<int>> regions;
    for (int fi : faces) regions[uf.find(fi)].insert(fi);
    struct Straight {
        int p, a, b; // a point left in line between its outline neighbours
    };
    std::set<int> gone;
    std::vector<std::set<int>> shrink;
    std::vector<Straight> straight;
    std::vector<int> made;
    for (const auto& [root, region] : regions) {
        (void)root;
        if (region.size() == 1) {
            const Face& f = m.faces[*region.begin()];
            shrink.push_back(std::set<int>(f.v, f.v + f.n));
            continue;
        }
        const std::vector<int> loop = outline(e.mesh(), region);
        if (loop.empty()) return fail("the faces would make a ring (an outline with a hole)");
        // Outline points on an edge between two of the joined faces, in line with their
        // neighbours on the outline: they go afterwards.
        std::set<int> inner;
        for (int fi : region) {
            const Face& f = m.faces[fi];
            for (int k = 0; k < f.n; ++k) {
                const int x = f.v[k], y = f.v[(k + 1) % f.n];
                auto it = owner.find({y, x});
                if (it != owner.end() && region.count(it->second)) inner.insert(x), inner.insert(y);
            }
        }
        const int L = (int)loop.size();
        for (int i = 0; i < L; ++i) {
            const int p = loop[i], a = loop[(i + L - 1) % L], b = loop[(i + 1) % L];
            if (inner.count(p) && straightBetween(m, p, a, b)) straight.push_back({p, a, b});
        }
        if (!rejoin(e, region, loop, gone, &made)) return fail("the faces are too bent to join");
    }
    // A point also on a face that shrinks stays.
    for (const auto& g : shrink)
        straight.erase(std::remove_if(straight.begin(), straight.end(), [&](const Straight& s) { return g.count(s.p) > 0; }),
                       straight.end());
    std::vector<int> fmap, vmap;
    e.deleteFaces(gone, &fmap, &vmap);
    std::set<int> kept;
    for (int f : made)
        if (fmap[f] >= 0) kept.insert(fmap[f]);
    // Points are followed by index through every renumbering.
    auto remap = [&](const std::vector<int>& vm) {
        for (auto& g : shrink) {
            std::set<int> h;
            for (int v : g) h.insert(vm[v]);
            g = h;
        }
        auto at = [&](int v) { return v >= 0 ? vm[v] : -1; };
        for (auto& s : straight) s = {at(s.p), at(s.a), at(s.b)};
    };
    remap(vmap);
    // The points left in line go; the faces beyond them get their old shape back.
    for (size_t i = 0; i < straight.size(); ++i) {
        const Straight s = straight[i];
        if (s.p < 0 || s.a < 0 || s.b < 0) continue;
        std::vector<int> vm;
        if (!dropStraightPoint(e, s.p, s.a, s.b, kept, vm)) continue; // kept as a corner
        // A neighbour further along the same line now ends where this one ended.
        for (size_t j = i + 1; j < straight.size(); ++j) {
            if (straight[j].a == s.p) straight[j].a = s.a;
            if (straight[j].b == s.p) straight[j].b = s.b;
        }
        remap(vm);
    }
    // A face alone shrinks to a point.
    for (const auto& g : shrink) mergeRaw(e, g, centreOf(e.mesh(), g), *g.begin());
    if (!shrink.empty()) {
        tidy(e);
        kept.clear(); // indices moved; nothing left selected
    }
    if (out) *out = kept;
    if (closed && !isClosed(e.mesh())) return fail("it would open the model");
    return true;
}

// ---------------------------------------------------------------- fill, detach, flips

bool fillFace(MeshEdit& e, const std::set<int>& verts, int* made, std::string* why) {
    auto fail = [&](const std::string& w) {
        if (why) *why = w;
        return false;
    };
    if (verts.size() != 3 && verts.size() != 4) return fail("select 3 or 4 points");
    const Mesh& m = e.mesh();
    std::vector<int> v(verts.begin(), verts.end());
    // Four points: the order that doesn't cross itself (the largest area).
    if (v.size() == 4) {
        const std::vector<std::vector<int>> orders = {{v[0], v[1], v[2], v[3]}, {v[0], v[1], v[3], v[2]}, {v[0], v[2], v[1], v[3]}};
        double best = -1;
        std::vector<int> pick;
        for (const auto& o : orders) {
            Vec3 n{0, 0, 0};
            for (int k = 0; k < 4; ++k) n = n + cross(m.verts[o[k]], m.verts[o[(k + 1) % 4]]);
            if (length(n) > best) best = length(n), pick = o;
        }
        v = pick;
    }
    if (area(m, v) < kMinArea) return fail("the points are in a line");
    // Turned to match the faces round it: an edge already running a -> b means this face
    // must run b -> a.
    std::set<std::pair<int, int>> dir;
    for (const Face& f : m.faces)
        for (int k = 0; k < f.n; ++k) dir.insert({f.v[k], f.v[(k + 1) % f.n]});
    int same = 0, opposite = 0;
    for (size_t k = 0; k < v.size(); ++k) {
        const int a = v[k], b = v[(k + 1) % v.size()];
        same += (int)dir.count({a, b});
        opposite += (int)dir.count({b, a});
    }
    if (same > opposite) std::reverse(v.begin(), v.end());
    else if (same == 0 && opposite == 0) {
        // Nothing round it: face the outside of the model (away from its middle).
        Vec3 mn, mx;
        if (m.bounds(mn, mx)) {
            const Vec3 mid = (mn + mx) * 0.5;
            Vec3 n{0, 0, 0};
            for (size_t k = 0; k < v.size(); ++k) n = n + cross(m.verts[v[k]], m.verts[v[(k + 1) % v.size()]]);
            Vec3 c{0, 0, 0};
            for (int x : v) c = c + m.verts[x];
            c = c * (1.0 / v.size());
            if (dot(n, c - mid) < 0) std::reverse(v.begin(), v.end());
        }
    }
    // Not over an existing face.
    std::vector<int> canon = v;
    std::sort(canon.begin(), canon.end());
    for (const Face& f : m.faces) {
        std::vector<int> c(f.v, f.v + f.n);
        std::sort(c.begin(), c.end());
        if (c == canon) return fail("there is a face there already");
    }
    Face f;
    f.n = (int)v.size();
    for (int k = 0; k < f.n; ++k) f.v[k] = v[k];
    const int idx = e.addFace(f);
    if (made) *made = idx;
    return true;
}

bool detachFaces(MeshEdit& e, const std::set<int>& faces) {
    if (faces.empty()) return false;
    const Mesh& m = e.mesh();
    std::set<int> inSel, outSel;
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Face& f = m.faces[fi];
        for (int k = 0; k < f.n; ++k) (faces.count(fi) ? inSel : outSel).insert(f.v[k]);
    }
    std::map<int, int> copy;
    for (int v : inSel)
        if (outSel.count(v)) copy[v] = e.addVert(m.verts[v]);
    if (copy.empty()) return false;
    for (int fi : faces) {
        Face f = e.mesh().faces[fi];
        for (int k = 0; k < f.n; ++k) {
            auto it = copy.find(f.v[k]);
            if (it != copy.end()) f.v[k] = it->second;
        }
        e.setFace(fi, f);
    }
    return true;
}

bool flipFaces(MeshEdit& e, const std::set<int>& faces) {
    for (int fi : faces) e.setFace(fi, flipped(e.mesh().faces[fi]));
    return !faces.empty();
}

bool flipDiagonals(MeshEdit& e, const std::set<int>& faces) {
    bool any = false;
    for (int fi : faces) {
        Face f = e.mesh().faces[fi];
        if (f.n != 4) continue;
        f.diag = !f.diag;
        e.setFace(fi, f);
        any = true;
    }
    return any;
}

bool triangulate(MeshEdit& e, const std::set<int>& faces, std::set<int>* out) {
    bool any = false;
    std::set<int> made;
    for (int fi : faces) {
        const Face f = e.mesh().faces[fi];
        if (f.n != 4) {
            made.insert(fi);
            continue;
        }
        // The same split as drawn.
        const int a = f.v[0], b = f.v[1], c = f.v[2], d = f.v[3];
        const std::vector<int> t1 = f.diag ? std::vector<int>{a, b, d} : std::vector<int>{a, b, c};
        const std::vector<int> t2 = f.diag ? std::vector<int>{b, c, d} : std::vector<int>{a, c, d};
        e.setFace(fi, faceFrom(f, t1));
        made.insert(fi);
        made.insert(e.addFace(faceFrom(f, t2)));
        any = true;
    }
    if (out) *out = made;
    return any;
}

int joinIntoQuads(MeshEdit& e, const std::set<int>& faces, std::set<int>* out) {
    const Mesh& m = e.mesh();
    struct Cand {
        int t, s;
        std::vector<int> quad;
        double score;
    };
    std::map<std::pair<int, int>, int> owner;
    for (int fi : faces)
        if (m.faces[fi].n == 3)
            for (int k = 0; k < 3; ++k) owner[{m.faces[fi].v[k], m.faces[fi].v[(k + 1) % 3]}] = fi;
    std::vector<Cand> cands;
    for (int t : faces) {
        const Face& ft = m.faces[t];
        if (ft.n != 3) continue;
        for (int i = 0; i < 3; ++i) {
            const int x = ft.v[i], y = ft.v[(i + 1) % 3], p = ft.v[(i + 2) % 3];
            auto it = owner.find({y, x});
            if (it == owner.end() || it->second <= t) continue;
            const int s = it->second;
            const Face& fs = m.faces[s];
            int q = -1;
            for (int k = 0; k < 3; ++k)
                if (fs.v[k] != x && fs.v[k] != y) q = fs.v[k];
            const std::vector<int> quad = {x, q, y, p};
            // Flat enough (the two triangles' normals within ~10°) and convex.
            const Vec3 n1 = faceNormal(m, t), n2 = faceNormal(m, s);
            const double flat = dot(n1, n2);
            if (flat < 0.985) continue;
            const Vec3 n = (n1 + n2) * 0.5;
            double worst = 1;
            for (int k = 0; k < 4; ++k) {
                const Vec3 a = m.verts[quad[(k + 3) % 4]], b = m.verts[quad[k]], c = m.verts[quad[(k + 1) % 4]];
                const Vec3 e1 = b - a, e2 = c - b;
                const double l = length(e1) * length(e2);
                worst = std::min(worst, l > 0 ? dot(cross(e1, e2), n) / l : -1);
            }
            if (worst < 0.05) continue;
            cands.push_back({t, s, quad, worst + flat});
        }
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.score > b.score; });
    std::set<int> used, gone, made;
    for (const Cand& c : cands) {
        if (used.count(c.t) || used.count(c.s)) continue;
        used.insert(c.t);
        used.insert(c.s);
        e.setFace(c.t, faceFrom(m.faces[c.t], c.quad));
        gone.insert(c.s);
        made.insert(c.t);
    }
    std::vector<int> map;
    e.deleteFaces(gone, &map);
    if (out) {
        out->clear();
        for (int f : faces)
            if (f < (int)map.size() && map[f] >= 0) out->insert(map[f]);
    }
    return (int)made.size();
}

bool setTwoSided(MeshEdit& e, const std::set<int>& faces, bool on) {
    for (int fi : faces) {
        Face f = e.mesh().faces[fi];
        f.twoSided = on;
        e.setFace(fi, f);
    }
    return !faces.empty();
}

} // namespace mazz
