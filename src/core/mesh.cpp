#include "core/mesh.hpp"
#include "core/numbers.hpp"
#include <algorithm>
#include <numeric>

namespace mazz {

bool Face::operator==(const Face& o) const {
    if (n != o.n || tex != o.tex || group != o.group || sg != o.sg || twoSided != o.twoSided || diag != o.diag)
        return false;
    for (int k = 0; k < n; ++k)
        if (v[k] != o.v[k] || !(uv[k] == o.uv[k]) || !(col[k] == o.col[k])) return false;
    return true;
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

Vec3 faceNormal(const Mesh& m, int face) {
    const Face& f = m.faces[face];
    // Newell's method: robust for slightly bent quads.
    Vec3 n;
    for (int k = 0; k < f.n; ++k) {
        const Vec3& a = m.verts[f.v[k]];
        const Vec3& b = m.verts[f.v[(k + 1) % f.n]];
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    return normalize(n);
}

Vec3 faceCentre(const Mesh& m, int face) {
    const Face& f = m.faces[face];
    Vec3 c;
    for (int k = 0; k < f.n; ++k) c += m.verts[f.v[k]];
    return c / f.n;
}

namespace {
int findRoot(std::vector<int>& p, int i) {
    while (p[i] != i) i = p[i] = p[p[i]];
    return i;
}
} // namespace

int pieceIds(const Mesh& m, std::vector<int>& pieceOfFace) {
    // Union-find over points; a face joins all its points.
    std::vector<int> parent(m.verts.size());
    std::iota(parent.begin(), parent.end(), 0);
    for (const Face& f : m.faces)
        for (int k = 1; k < f.n; ++k) {
            const int a = findRoot(parent, f.v[0]), b = findRoot(parent, f.v[k]);
            if (a != b) parent[std::max(a, b)] = std::min(a, b);
        }
    std::vector<int> number(m.verts.size(), -1);
    pieceOfFace.assign(m.faces.size(), -1);
    int count = 0;
    for (size_t i = 0; i < m.faces.size(); ++i) {
        const int r = findRoot(parent, m.faces[i].v[0]);
        if (number[r] < 0) number[r] = count++;
        pieceOfFace[i] = number[r];
    }
    return count;
}

void addBox(Mesh& m, const Vec3& a, const Vec3& b) {
    Vec3 mn, mx;
    for (int k = 0; k < 3; ++k) {
        mn[k] = clean(std::min(a[k], b[k]));
        mx[k] = clean(std::max(a[k], b[k]));
    }
    const int base = (int)m.verts.size();
    // Corner i: bit 0 = x max, bit 1 = y max, bit 2 = z max.
    for (int i = 0; i < 8; ++i)
        m.verts.push_back({(i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z});
    // Counter-clockwise seen from outside.
    static const int quads[6][4] = {
        {0, 2, 3, 1}, // bottom (-z)
        {4, 5, 7, 6}, // top (+z)
        {0, 1, 5, 4}, // front (-y)
        {2, 6, 7, 3}, // back (+y)
        {0, 4, 6, 2}, // left (-x)
        {1, 3, 7, 5}, // right (+x)
    };
    for (const auto& q : quads) {
        Face f;
        f.n = 4;
        for (int k = 0; k < 4; ++k) f.v[k] = base + q[k];
        m.faces.push_back(f);
    }
}

} // namespace mazz
