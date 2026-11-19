#include "core/mesh.hpp"
#include <algorithm>
#include <cmath>
#include <map>

namespace mazz {

int Face::triangles(std::array<std::array<int, 3>, 2>& out) const {
    if (n == 3) {
        out[0] = {0, 1, 2};
        return 1;
    }
    if (!diag) {
        out[0] = {0, 1, 2};
        out[1] = {0, 2, 3};
    } else {
        out[0] = {0, 1, 3};
        out[1] = {1, 2, 3};
    }
    return 2;
}

int Mesh::addTri(int a, int b, int c, int sg) {
    Face f;
    f.n = 3;
    f.v = {a, b, c, 0};
    f.sg = sg;
    faces.push_back(f);
    return (int)faces.size() - 1;
}

int Mesh::addQuad(int a, int b, int c, int d, int sg) {
    Face f;
    f.n = 4;
    f.v = {a, b, c, d};
    f.sg = sg;
    faces.push_back(f);
    return (int)faces.size() - 1;
}

int Mesh::append(const Mesh& other) {
    const int base = (int)verts.size();
    const int firstFace = (int)faces.size();
    verts.insert(verts.end(), other.verts.begin(), other.verts.end());
    for (Face f : other.faces) {
        for (int k = 0; k < f.n; ++k) f.v[k] += base;
        faces.push_back(f);
    }
    return firstFace;
}

int Mesh::triangleCount(bool includeHidden) const {
    int t = 0;
    for (const Face& f : faces)
        if (includeHidden || !f.hidden) t += f.n == 4 ? 2 : 1;
    return t;
}

Vec3 Mesh::faceNormal(int fi) const {
    const Face& f = faces[fi];
    // Newell's method: robust for slightly non-planar quads.
    Vec3 n;
    for (int k = 0; k < f.n; ++k) {
        const Vec3& a = verts[f.v[k]];
        const Vec3& b = verts[f.v[(k + 1) % f.n]];
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    return normalize(n);
}

Vec3 Mesh::faceCentre(int fi) const {
    const Face& f = faces[fi];
    Vec3 c;
    for (int k = 0; k < f.n; ++k) c += verts[f.v[k]];
    return c / f.n;
}

double Mesh::faceArea(int fi) const {
    const Face& f = faces[fi];
    std::array<std::array<int, 3>, 2> tris;
    const int nt = f.triangles(tris);
    double a = 0;
    for (int t = 0; t < nt; ++t) {
        const Vec3& p0 = verts[f.v[tris[t][0]]];
        const Vec3& p1 = verts[f.v[tris[t][1]]];
        const Vec3& p2 = verts[f.v[tris[t][2]]];
        a += length(cross(p1 - p0, p2 - p0)) / 2;
    }
    return a;
}

bool Mesh::bounds(Vec3& mn, Vec3& mx) const {
    if (verts.empty()) return false;
    mn = mx = verts[0];
    for (const Vec3& p : verts)
        for (int a = 0; a < 3; ++a) {
            mn[a] = std::min(mn[a], p[a]);
            mx[a] = std::max(mx[a], p[a]);
        }
    return true;
}

std::vector<Edge> Mesh::edges(bool includeHidden) const {
    std::vector<Edge> e;
    for (const Face& f : faces) {
        if (!includeHidden && f.hidden) continue;
        for (int k = 0; k < f.n; ++k) e.push_back(makeEdge(f.v[k], f.v[(k + 1) % f.n]));
    }
    std::sort(e.begin(), e.end());
    e.erase(std::unique(e.begin(), e.end()), e.end());
    return e;
}

int Mesh::removeUnusedVertices() {
    std::vector<int> used(verts.size(), 0);
    for (const Face& f : faces)
        for (int k = 0; k < f.n; ++k) used[f.v[k]] = 1;
    std::vector<int> remap(verts.size(), -1);
    std::vector<Vec3> nv;
    for (size_t i = 0; i < verts.size(); ++i)
        if (used[i]) {
            remap[i] = (int)nv.size();
            nv.push_back(verts[i]);
        }
    const int removed = (int)(verts.size() - nv.size());
    verts.swap(nv);
    for (Face& f : faces)
        for (int k = 0; k < f.n; ++k) f.v[k] = remap[f.v[k]];
    return removed;
}

void Mesh::removeFaces(const std::vector<bool>& remove) {
    std::vector<Face> nf;
    for (size_t i = 0; i < faces.size(); ++i)
        if (i >= remove.size() || !remove[i]) nf.push_back(faces[i]);
    faces.swap(nf);
    removeUnusedVertices();
}

MeshCheck checkMesh(const Mesh& m, double minArea) {
    MeshCheck c;
    for (size_t i = 0; i < m.faces.size(); ++i) {
        const Face& f = m.faces[i];
        if (f.n != 3 && f.n != 4) { ++c.badCornerCount; continue; }
        bool bad = false;
        for (int k = 0; k < f.n; ++k)
            if (f.v[k] < 0 || f.v[k] >= (int)m.verts.size()) bad = true;
        if (bad) { ++c.badIndices; continue; }
        bool rep = false;
        for (int a = 0; a < f.n; ++a)
            for (int b = a + 1; b < f.n; ++b)
                if (f.v[a] == f.v[b]) rep = true;
        if (rep) { ++c.repeatedCorners; continue; }
        if (m.faceArea((int)i) <= minArea) ++c.zeroArea;
    }
    return c;
}

bool meshOk(const Mesh& m) {
    const MeshCheck c = checkMesh(m);
    return c.badIndices == 0 && c.repeatedCorners == 0 && c.zeroArea == 0 && c.badCornerCount == 0;
}

bool isClosed(const Mesh& m) {
    // Directed edge counts: each directed edge once, and its reverse once.
    std::map<std::pair<int, int>, int> directed;
    for (const Face& f : m.faces)
        for (int k = 0; k < f.n; ++k) ++directed[{f.v[k], f.v[(k + 1) % f.n]}];
    for (const auto& [e, count] : directed) {
        if (count != 1) return false;
        auto it = directed.find({e.second, e.first});
        if (it == directed.end() || it->second != 1) return false;
    }
    return !m.faces.empty();
}

double signedVolume(const Mesh& m) {
    double v = 0;
    for (const Face& f : m.faces) {
        std::array<std::array<int, 3>, 2> tris;
        const int nt = f.triangles(tris);
        for (int t = 0; t < nt; ++t) {
            const Vec3& a = m.verts[f.v[tris[t][0]]];
            const Vec3& b = m.verts[f.v[tris[t][1]]];
            const Vec3& c = m.verts[f.v[tris[t][2]]];
            v += dot(a, cross(b, c)) / 6.0;
        }
    }
    return v;
}

} // namespace mazz
