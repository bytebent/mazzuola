#include "core/model_ops.hpp"
#include "core/numbers.hpp"
#include "core/ops.hpp"
#include <algorithm>
#include <cmath>
#include <map>

namespace mazz {

namespace {

constexpr double kTiny = 1e-6;       // cm² / cm: smaller areas and lengths count as nothing
constexpr double kMaxStretch = 4.0;  // corner offsets never grow beyond 4× the distance

using DirEdge = std::pair<int, int>; // a -> b, as a face goes round

// Twice the area vector of a face (Newell), not normalised.
Vec3 areaVector(const Mesh& m, const Face& f) {
    Vec3 n{0, 0, 0};
    for (int k = 0; k < f.n; ++k) {
        const Vec3& a = m.verts[f.v[k]];
        const Vec3& b = m.verts[f.v[(k + 1) % f.n]];
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    return n;
}

Vec3 unit(const Vec3& v) {
    const double l = length(v);
    return l > 0 ? v * (1.0 / l) : Vec3{0, 0, 0};
}

// A face is usable: some area, and (if `was` is given) still facing roughly the same way.
bool faceOk(const Mesh& m, const Face& f, const Vec3* was = nullptr) {
    const Vec3 a = areaVector(m, f);
    if (length(a) * 0.5 < kTiny) return false;
    if (was && dot(unit(a), *was) < 0.5) return false;
    return true;
}

Face withVerts(const Face& like, std::initializer_list<int> vs) {
    Face f;
    f.tex = like.tex;
    f.group = like.group;
    f.sg = like.sg;
    f.twoSided = like.twoSided;
    f.n = (int)vs.size();
    int k = 0;
    for (int v : vs) f.v[k++] = v;
    return f;
}

std::set<DirEdge> directedEdges(const Mesh& m, const std::set<int>& faces) {
    std::set<DirEdge> s;
    for (int fi : faces) {
        const Face& f = m.faces[fi];
        for (int k = 0; k < f.n; ++k) s.insert({f.v[k], f.v[(k + 1) % f.n]});
    }
    return s;
}

// The region's boundary: directed edges whose reverse is not in the region, with the
// face they belong to.
std::vector<std::pair<DirEdge, int>> boundary(const Mesh& m, const std::set<int>& faces) {
    const std::set<DirEdge> all = directedEdges(m, faces);
    std::vector<std::pair<DirEdge, int>> out;
    for (int fi : faces) {
        const Face& f = m.faces[fi];
        for (int k = 0; k < f.n; ++k) {
            const DirEdge d{f.v[k], f.v[(k + 1) % f.n]};
            if (!all.count({d.second, d.first})) out.push_back({d, fi});
        }
    }
    return out;
}

// Per point of the region: the direction that moves every face it touches by one unit
// along that face's normal (two faces at right angles: the corner moves diagonally).
std::map<int, Vec3> regionNormals(const Mesh& m, const std::set<int>& faces) {
    std::map<int, std::vector<Vec3>> normals;
    for (int fi : faces) {
        const Vec3 n = faceNormal(m, fi);
        if (length(n) == 0) continue;
        const Face& f = m.faces[fi];
        for (int k = 0; k < f.n; ++k) {
            auto& list = normals[f.v[k]];
            bool have = false;
            for (const Vec3& o : list)
                if (length(o - n) < 1e-9) have = true;
            if (!have) list.push_back(n);
        }
    }
    std::map<int, Vec3> out;
    for (auto& [v, list] : normals) {
        Vec3 sum{0, 0, 0};
        for (const Vec3& n : list) sum = sum + n;
        const Vec3 avg = sum * (1.0 / list.size());
        // Along the average, as far as makes each face move one unit: avg / |avg|².
        const double len = length(avg);
        if (len < 1e-9) out[v] = list[0];
        else if (1.0 / len > kMaxStretch) out[v] = unit(avg) * kMaxStretch;
        else out[v] = avg * (1.0 / (len * len));
    }
    return out;
}

// The faces of a set that use any of `verts`.
std::set<int> facesUsing(const Mesh& m, const std::set<int>& verts) {
    std::set<int> out;
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Face& f = m.faces[fi];
        for (int k = 0; k < f.n; ++k)
            if (verts.count(f.v[k])) {
                out.insert(fi);
                break;
            }
    }
    return out;
}

// Extrudes one region by a per-point offset.
bool extrudeRegion(MeshEdit& e, const std::set<int>& region, const std::map<int, Vec3>& offset, bool keepOriginal,
                   std::set<int>* out) {
    const Mesh& m = e.mesh();
    // Which way the region goes, seen from its faces: out (+) or in (-).
    double sideSum = 0;
    for (int fi : region) {
        const Face& f = m.faces[fi];
        const Vec3 n = faceNormal(m, fi);
        for (int k = 0; k < f.n; ++k) sideSum += dot(offset.at(f.v[k]), n);
    }
    if (std::fabs(sideSum) < kTiny) return false;
    const bool inward = sideSum < 0;
    const auto edges = boundary(m, region);
    std::map<int, int> moved;
    for (const auto& [v, off] : offset) moved[v] = e.addVert(m.verts[v] + off);
    // Sides: a -> b on the region's edge, the new points beyond. They face the right way
    // whichever way the region goes, except when the original stays behind and the
    // region goes in (then the original is the outside).
    const bool flipSides = keepOriginal && inward;
    std::vector<int> sides;
    for (const auto& [d, fi] : edges) {
        const Face like = m.faces[fi];
        const int a = d.first, b = d.second;
        Face s = withVerts(like, {a, b, moved.at(b), moved.at(a)});
        if (flipSides) s = flipped(s);
        sides.push_back(e.addFace(s));
    }
    std::set<int> ends;
    std::vector<std::pair<int, Vec3>> check; // faces and the way they should face
    for (int fi : region) {
        const Face orig = m.faces[fi];
        const Vec3 n = faceNormal(m, fi);
        Face nf = orig;
        for (int k = 0; k < nf.n; ++k) nf.v[k] = moved.at(orig.v[k]);
        if (keepOriginal) {
            // The original stays: it becomes the other side of the solid.
            if (inward) {
                nf = flipped(nf);
                ends.insert(e.addFace(nf));
                check.push_back({*ends.rbegin(), n * -1.0});
            } else {
                e.setFace(fi, flipped(orig));
                ends.insert(e.addFace(nf));
                check.push_back({*ends.rbegin(), n});
            }
        } else {
            e.setFace(fi, nf);
            ends.insert(fi);
            check.push_back({fi, n});
        }
    }
    // Every original point the region no longer uses and nothing else uses is dropped
    // by the caller's cleanup; here, check the result.
    const Mesh& r = e.mesh();
    for (int s : sides)
        if (!faceOk(r, r.faces[s])) return false;
    for (const auto& [fi, n] : check)
        if (!faceOk(r, r.faces[fi], &n)) return false;
    if (out) *out = ends;
    return true;
}

// Removes points no face uses (left behind by a region that moved away).
void dropUnused(MeshEdit& e) {
    const Mesh& m = e.mesh();
    std::vector<char> used(m.verts.size(), 0);
    for (const Face& f : m.faces)
        for (int k = 0; k < f.n; ++k) used[f.v[k]] = 1;
    bool any = false;
    for (char u : used) any |= !u;
    if (!any) return;
    // deleteFaces with nothing to delete still drops unused points.
    e.deleteFaces({});
}

} // namespace

// ---------------------------------------------------------------- checks

bool isClosed(const Mesh& m) {
    std::map<DirEdge, int> count;
    for (const Face& f : m.faces)
        for (int k = 0; k < f.n; ++k) ++count[{f.v[k], f.v[(k + 1) % f.n]}];
    for (const auto& [d, c] : count) {
        if (c != 1) return false;
        auto it = count.find({d.second, d.first});
        if (it == count.end() || it->second != 1) return false;
    }
    return !m.faces.empty();
}

double signedVolume(const Mesh& m) {
    double v = 0;
    for (const Face& f : m.faces)
        for (int k = 1; k + 1 < f.n; ++k) {
            const Vec3& a = m.verts[f.v[0]];
            const Vec3& b = m.verts[f.v[k]];
            const Vec3& c = m.verts[f.v[k + 1]];
            v += dot(a, cross(b, c));
        }
    return v / 6.0;
}

// ---------------------------------------------------------------- push/pull

PushKind pushKind(const Mesh& m, int face, bool forceNew) {
    const std::set<DirEdge> mine = directedEdges(m, {face});
    std::set<DirEdge> others;
    for (int fi = 0; fi < (int)m.faces.size(); ++fi)
        if (fi != face) {
            const Face& f = m.faces[fi];
            for (int k = 0; k < f.n; ++k) others.insert({f.v[k], f.v[(k + 1) % f.n]});
        }
    for (const DirEdge& d : mine)
        if (!others.count({d.second, d.first})) return PushKind::Solid;
    if (forceNew) return PushKind::NewFaces;
    const Vec3 n = faceNormal(m, face);
    std::set<int> verts;
    const Face& f = m.faces[face];
    for (int k = 0; k < f.n; ++k) verts.insert(f.v[k]);
    for (int fi : facesUsing(m, verts)) {
        if (fi == face) continue;
        const Vec3 o = faceNormal(m, fi);
        if (length(o) == 0 || std::fabs(dot(o, n)) > 1e-6) return PushKind::NewFaces;
    }
    return PushKind::Stretch;
}

bool pushPull(MeshEdit& e, int face, double distance, bool forceNew, int* moved) {
    const Mesh& m = e.mesh();
    if (face < 0 || face >= (int)m.faces.size() || std::fabs(distance) < kTiny) return false;
    const Vec3 n = faceNormal(m, face);
    if (length(n) == 0) return false;
    switch (pushKind(m, face, forceNew)) {
    case PushKind::Stretch: {
        std::set<int> verts;
        const Face f = m.faces[face];
        for (int k = 0; k < f.n; ++k) verts.insert(f.v[k]);
        const std::set<int> around = facesUsing(m, verts);
        std::map<int, Vec3> before;
        for (int fi : around) before[fi] = faceNormal(m, fi);
        for (int v : verts) e.setVert(v, m.verts[v] + n * distance);
        for (int fi : around)
            if (!faceOk(e.mesh(), e.mesh().faces[fi], &before[fi])) return false;
        if (moved) *moved = face;
        return true;
    }
    case PushKind::NewFaces: {
        ExtrudeParams p;
        p.distance = distance;
        std::set<int> out;
        if (!extrudeFaces(e, {face}, p, &out)) return false;
        if (moved) *moved = *out.begin();
        return true;
    }
    case PushKind::Solid: {
        ExtrudeParams p;
        p.distance = distance;
        p.keepOriginal = true;
        std::set<int> out;
        if (!extrudeFaces(e, {face}, p, &out)) return false;
        if (moved) *moved = *out.begin();
        return true;
    }
    }
    return false;
}

Vec3 surfaceNormal(const Mesh& m, const std::set<int>& verts, bool edge) {
    Vec3 sum{0, 0, 0};
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Face& f = m.faces[fi];
        int uses = 0;
        for (int k = 0; k < f.n; ++k) uses += (int)verts.count(f.v[k]);
        if (uses == 0 || (edge && uses < 2)) continue;
        sum = sum + faceNormal(m, fi);
    }
    return length(sum) < 1e-9 ? Vec3{0, 0, 0} : unit(sum);
}

bool pushPoints(MeshEdit& e, const std::set<int>& verts, const Vec3& dir, double distance) {
    const Mesh& m = e.mesh();
    if (verts.empty() || length(dir) < 1e-9 || std::fabs(distance) < kTiny) return false;
    const std::set<int> around = facesUsing(m, verts);
    std::map<int, Vec3> before;
    for (int fi : around) before[fi] = faceNormal(m, fi);
    for (int v : verts) e.setVert(v, m.verts[v] + dir * distance);
    for (int fi : around) {
        const Face& f = e.mesh().faces[fi];
        if (length(areaVector(e.mesh(), f)) * 0.5 < kTiny) return false;
        if (dot(unit(areaVector(e.mesh(), f)), before[fi]) <= 0) return false; // turned over
    }
    return true;
}

// ---------------------------------------------------------------- extrude

bool extrudeFaces(MeshEdit& e, const std::set<int>& faces, const ExtrudeParams& p, std::set<int>* out) {
    if (faces.empty() || std::fabs(p.distance) < kTiny) return false;
    std::vector<std::set<int>> regions;
    if (p.each)
        for (int f : faces) regions.push_back({f});
    else
        regions.push_back(faces);
    // Work out every region's offsets before anything moves.
    std::vector<std::map<int, Vec3>> offsets;
    Vec3 dir{0, 0, 0};
    (p.axis == 0 ? dir.x : p.axis == 1 ? dir.y : dir.z) = 1;
    for (const auto& r : regions) {
        std::map<int, Vec3> off;
        if (p.straight) {
            for (int fi : r) {
                const Face& f = e.mesh().faces[fi];
                for (int k = 0; k < f.n; ++k) off[f.v[k]] = dir * p.distance;
            }
        } else {
            for (const auto& [v, n] : regionNormals(e.mesh(), r)) off[v] = n * p.distance;
        }
        offsets.push_back(off);
    }
    std::set<int> ends;
    for (size_t i = 0; i < regions.size(); ++i) {
        std::set<int> got;
        if (!extrudeRegion(e, regions[i], offsets[i], p.keepOriginal, &got)) return false;
        ends.insert(got.begin(), got.end());
    }
    // Points left behind by regions that moved away: faces keep their indices (only
    // points go), so the end faces stay valid.
    dropUnused(e);
    if (out) *out = ends;
    return true;
}

std::set<Edge> openEdges(const Mesh& m, const std::set<Edge>& edges) {
    std::map<Edge, int> uses;
    for (const Face& f : m.faces)
        for (int k = 0; k < f.n; ++k) ++uses[makeEdge(f.v[k], f.v[(k + 1) % f.n])];
    std::set<Edge> out;
    for (const Edge& ed : edges) {
        auto it = uses.find(ed);
        if (it != uses.end() && it->second == 1) out.insert(ed);
    }
    return out;
}

bool extrudeEdges(MeshEdit& e, const std::set<Edge>& edges, const Vec3& offset, std::set<Edge>* out) {
    if (length(offset) < kTiny) return false;
    const std::set<Edge> open = openEdges(e.mesh(), edges);
    if (open.empty()) return false;
    // The face each open edge belongs to, and which way it runs there.
    std::map<Edge, std::pair<int, DirEdge>> owner;
    const Mesh& m = e.mesh();
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Face& f = m.faces[fi];
        for (int k = 0; k < f.n; ++k) {
            const DirEdge d{f.v[k], f.v[(k + 1) % f.n]};
            const Edge ed = makeEdge(d.first, d.second);
            if (open.count(ed)) owner[ed] = {fi, d};
        }
    }
    std::map<int, int> moved;
    for (const Edge& ed : open)
        for (int v : {ed.first, ed.second})
            if (!moved.count(v)) moved[v] = e.addVert(m.verts[v] + offset);
    std::set<Edge> far;
    std::vector<int> made;
    for (const auto& [ed, o] : owner) {
        const Face like = e.mesh().faces[o.first];
        const int a = o.second.first, b = o.second.second;
        // The new face continues the surface: it runs b -> a along the shared edge.
        made.push_back(e.addFace(withVerts(like, {b, a, moved[a], moved[b]})));
        far.insert(makeEdge(moved[a], moved[b]));
    }
    for (int f : made)
        if (!faceOk(e.mesh(), e.mesh().faces[f])) return false;
    if (out) *out = far;
    return true;
}

// ---------------------------------------------------------------- inset

bool inset(MeshEdit& e, const std::set<int>& faces, const InsetParams& p, std::set<int>* out) {
    if (faces.empty() || p.width < kTiny) return false;
    std::vector<std::set<int>> regions;
    if (p.each)
        for (int f : faces) regions.push_back({f});
    else
        regions.push_back(faces);
    std::set<int> inner;
    for (const auto& region : regions) {
        const Mesh& m = e.mesh();
        const auto edges = boundary(m, region);
        if (edges.empty()) return false;
        // Inward direction of each boundary edge, in its face's plane.
        std::map<int, std::vector<Vec3>> inwardAt; // point -> inward normals of its two boundary edges
        std::map<int, Vec3> faceN;
        for (int fi : region) faceN[fi] = faceNormal(m, fi);
        for (const auto& [d, fi] : edges) {
            const Vec3 along = unit(m.verts[d.second] - m.verts[d.first]);
            const Vec3 in = unit(cross(faceN[fi], along));
            inwardAt[d.first].push_back(in);
            inwardAt[d.second].push_back(in);
        }
        // New inner points for the boundary; the rest of the region only goes down.
        std::map<int, int> nv;
        std::map<int, Vec3> target;
        for (const auto& [v, ins] : inwardAt) {
            if (ins.size() != 2) return false; // the region touches itself at a point
            const Vec3& m1 = ins[0];
            const Vec3& m2 = ins[1];
            const double c = 1 + dot(m1, m2);
            if (c < 1e-6) return false; // folds back on itself
            const Vec3 off = (m1 + m2) * (p.width / c);
            target[v] = m.verts[v] + off;
        }
        for (const auto& [v, t] : target) nv[v] = e.addVert(t);
        std::vector<int> ring;
        for (const auto& [d, fi] : edges) {
            const Face like = e.mesh().faces[fi];
            ring.push_back(e.addFace(withVerts(like, {d.first, d.second, nv[d.second], nv[d.first]})));
        }
        for (int fi : region) {
            Face f = e.mesh().faces[fi];
            for (int k = 0; k < f.n; ++k)
                if (nv.count(f.v[k])) f.v[k] = nv[f.v[k]];
            e.setFace(fi, f);
        }
        // Too wide: an inner edge turned round, or a face vanished or flipped.
        const Mesh& r = e.mesh();
        for (const auto& [d, fi] : edges) {
            const Vec3 was = r.verts[d.second] - r.verts[d.first];
            const Vec3 now = r.verts[nv[d.second]] - r.verts[nv[d.first]];
            if (dot(was, now) <= kTiny) return false;
        }
        for (int fi : region)
            if (!faceOk(r, r.faces[fi], &faceN[fi])) return false;
        for (int f : ring)
            if (!faceOk(r, r.faces[f])) return false;
        inner.insert(region.begin(), region.end());
    }
    // Depth: the inner faces go in (or out) with straight walls, leaving a flat frame.
    if (std::fabs(p.depth) >= kTiny) {
        ExtrudeParams x;
        x.each = p.each;
        x.distance = -p.depth;
        if (!extrudeFaces(e, inner, x)) return false;
    }
    if (out) *out = inner;
    return true;
}

} // namespace mazz
