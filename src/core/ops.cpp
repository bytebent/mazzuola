#include "core/ops.hpp"
#include "core/select_ops.hpp"
#include "core/transform.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace mazz {

namespace {

// Faces whose corners are all in `pts` (they turn round when mirrored).
std::set<int> facesWithin(const Mesh& m, const std::set<int>& pts) {
    std::set<int> out;
    for (int i = 0; i < (int)m.faces.size(); ++i) {
        bool all = true;
        for (int k = 0; k < m.faces[i].n && all; ++k) all = pts.count(m.faces[i].v[k]) > 0;
        if (all) out.insert(i);
    }
    return out;
}

Vec3 rotateXYZ(const Vec3& p, const Vec3& pivot, const Vec3& deg) {
    Vec3 q = p;
    if (deg.x != 0) q = rotateAbout(q, pivot, {1, 0, 0}, deg.x);
    if (deg.y != 0) q = rotateAbout(q, pivot, {0, 1, 0}, deg.y);
    if (deg.z != 0) q = rotateAbout(q, pivot, {0, 0, 1}, deg.z);
    return q;
}

Vec3 scaleAbout(const Vec3& p, const Vec3& pivot, const Vec3& pct) {
    const Vec3 d = p - pivot;
    return pivot + Vec3{d.x * pct.x / 100, d.y * pct.y / 100, d.z * pct.z / 100};
}

bool mirrors(const Vec3& pct) { return (pct.x < 0) != ((pct.y < 0) != (pct.z < 0)); }

std::set<int> coveredFaces(const Mesh& m, const Selection& s) {
    return convertSelection(m, s, SelectMode::Face).faces;
}

} // namespace

Face flipped(const Face& f) {
    Face g = f;
    for (int k = 0; k < f.n; ++k) {
        g.v[k] = f.v[f.n - 1 - k];
        g.uv[k] = f.uv[f.n - 1 - k];
        g.col[k] = f.col[f.n - 1 - k];
    }
    return g;
}

Vec3 pivotPoint(const Mesh& m, const Selection& s, PivotKind kind, const Vec3& typed) {
    if (kind == PivotKind::Origin) return {0, 0, 0};
    if (kind == PivotKind::Typed) return typed;
    Vec3 mn, mx;
    if (!selectionBounds(m, s, mn, mx)) return {};
    return (mn + mx) / 2;
}

Vec3 selectionNormal(const Mesh& m, const Selection& s) {
    std::set<int> faces = coveredFaces(m, s);
    if (faces.empty()) {
        // Points or edges: the faces around them.
        const std::set<int> pts = selectedVerts(m, s);
        for (int i = 0; i < (int)m.faces.size(); ++i)
            for (int k = 0; k < m.faces[i].n; ++k)
                if (pts.count(m.faces[i].v[k])) faces.insert(i);
    }
    Vec3 n;
    for (int f : faces) n += faceNormal(m, f);
    return length(n) < 1e-9 ? Vec3{} : normalize(n);
}

bool applyTransform(MeshEdit& e, const Selection& s, const TransformParams& p) {
    const Mesh& m = e.mesh();
    const std::set<int> pts = selectedVerts(m, s);
    if (pts.empty()) return false;
    const Vec3 pivot = pivotPoint(m, s, p.pivot, p.typed);
    const Vec3 along = p.alongNormal != 0 ? selectionNormal(m, s) * p.alongNormal : Vec3{};
    const std::set<int> turnRound = mirrors(p.scale) ? facesWithin(m, pts) : std::set<int>{};
    std::vector<std::pair<int, Vec3>> next;
    for (int v : pts) {
        Vec3 q = scaleAbout(m.verts[v], pivot, p.scale);
        q = rotateXYZ(q, pivot, p.rotate);
        next.push_back({v, q + p.move + along});
    }
    for (const auto& [v, q] : next) e.setVert(v, q);
    for (int f : turnRound) e.setFace(f, flipped(e.mesh().faces[f]));
    return true;
}

std::set<int> replicate(MeshEdit& e, const Selection& s, const ReplicateParams& p) {
    const std::set<int> source = coveredFaces(e.mesh(), s);
    std::set<int> made;
    if (source.empty() || p.copies < 1) return made;
    // Where the source points start (copies are made from these).
    std::set<int> srcPts;
    for (int f : source)
        for (int k = 0; k < e.mesh().faces[f].n; ++k) srcPts.insert(e.mesh().faces[f].v[k]);
    std::vector<std::pair<int, Vec3>> prev; // the previous copy's points (index in src order)
    for (int v : srcPts) prev.push_back({v, e.mesh().verts[v]});
    Selection srcSel;
    srcSel.mode = SelectMode::Face;
    srcSel.faces = source;
    Vec3 pivot = pivotPoint(e.mesh(), srcSel, p.pivot, p.typed);
    Vec3 cumScale{1, 1, 1};
    bool flip = false;
    for (int k = 1; k <= p.copies; ++k) {
        // One step from the previous copy: scale and turn about the pivot, then offset.
        cumScale = {cumScale.x * p.scale.x / 100, cumScale.y * p.scale.y / 100, cumScale.z * p.scale.z / 100};
        if (mirrors(p.scale)) flip = !flip;
        const Vec3 off = p.scaleOffset ? Vec3{p.offset.x * cumScale.x, p.offset.y * cumScale.y, p.offset.z * cumScale.z} : p.offset;
        std::vector<std::pair<int, Vec3>> cur;
        for (const auto& [v, q] : prev) cur.push_back({v, rotateXYZ(scaleAbout(q, pivot, p.scale), pivot, p.turn) + off});
        if (p.pivot == PivotKind::Centre) pivot = pivot + off; // the pivot travels with the copies
        // The copy: new faces with new points at those positions.
        const std::set<int> faces = copyFaces(e, source);
        // copyFaces adds the points in source-face order; map original -> new by face corners.
        std::map<int, int> newOf;
        auto fit = faces.begin();
        for (int f : source) {
            const Face& orig = e.mesh().faces[f];
            const Face& copy = e.mesh().faces[*fit++];
            for (int c = 0; c < orig.n; ++c) newOf[orig.v[c]] = copy.v[c];
        }
        for (const auto& [v, q] : cur) e.setVert(newOf[v], q);
        if (flip)
            for (int f : faces) e.setFace(f, flipped(e.mesh().faces[f]));
        made.insert(faces.begin(), faces.end());
        prev = cur;
    }
    return made;
}

std::set<int> duplicate(MeshEdit& e, const Selection& s) { return copyFaces(e, coveredFaces(e.mesh(), s)); }

void mirrorInPlace(MeshEdit& e, const Selection& s, int axis) {
    TransformParams t;
    t.scale[axis] = -100;
    applyTransform(e, s, t);
}

std::set<int> mirrorClone(MeshEdit& e, const Selection& s, int axis) {
    const std::set<int> made = duplicate(e, s);
    Selection copy;
    copy.mode = SelectMode::Face;
    copy.faces = made;
    TransformParams t;
    t.scale[axis] = -100;
    t.pivot = PivotKind::Origin;
    applyTransform(e, copy, t);
    return made;
}

void flatten(MeshEdit& e, const Selection& s, int axis, FlattenTo to) {
    const std::set<int> pts = selectedVerts(e.mesh(), s);
    if (pts.empty()) return;
    double sum = 0, lo = 1e300, hi = -1e300;
    for (int v : pts) {
        const double c = e.mesh().verts[v][axis];
        sum += c;
        lo = std::min(lo, c);
        hi = std::max(hi, c);
    }
    const double target = to == FlattenTo::Lowest ? lo : to == FlattenTo::Highest ? hi : sum / pts.size();
    for (int v : pts) {
        Vec3 q = e.mesh().verts[v];
        q[axis] = target;
        e.setVert(v, q);
    }
}

} // namespace mazz
