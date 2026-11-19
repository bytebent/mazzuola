#include "core/primitives.hpp"
#include "core/rock.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <tuple>

namespace mazz {

const char* primKindName(PrimKind k) {
    static const char* n[] = {"Plane", "Box", "Cylinder", "Cone", "Tube", "Torus", "Sphere", "Geosphere", "Rock"};
    return n[(int)k];
}

bool primKindFromName(const std::string& s, PrimKind& out) {
    for (int i = 0; i < kPrimKindCount; ++i) {
        const std::string n = primKindName((PrimKind)i);
        if (n.size() != s.size()) continue;
        bool eq = true;
        for (size_t k = 0; k < n.size(); ++k)
            if (std::tolower((unsigned char)n[k]) != std::tolower((unsigned char)s[k])) eq = false;
        if (eq) { out = (PrimKind)i; return true; }
    }
    return false;
}

PrimParams clampParams(const PrimParams& in) {
    PrimParams p = in;
    for (int a = 0; a < 3; ++a) p.size[a] = std::max(0.01, std::fabs(p.size[a]));
    p.axis = std::clamp(p.axis, 0, 2);
    p.rows = std::clamp(p.rows, 1, 64);
    p.cols = std::clamp(p.cols, 1, 64);
    p.segX = std::clamp(p.segX, 1, 32);
    p.segY = std::clamp(p.segY, 1, 32);
    p.segZ = std::clamp(p.segZ, 1, 32);
    p.round = std::max(0.0, p.round);
    p.roundSegs = std::clamp(p.roundSegs, 1, 8);
    p.sides = std::clamp(p.sides, 3, 64);
    p.stacks = std::clamp(p.stacks, p.kind == PrimKind::Sphere ? 2 : 1, 64);
    p.capRings = std::clamp(p.capRings, 1, 16);
    p.inner = std::clamp(p.inner, 0.05, 0.95);
    p.rings = std::clamp(p.rings, 3, 64);
    p.iterations = std::clamp(p.iterations, 0, 4);
    p.faces = std::clamp(p.faces, 12, 60);
    p.roughness = std::clamp(p.roughness, 0.0, 100.0);
    for (int a = 0; a < 3; ++a) p.stretch[a] = std::clamp(p.stretch[a], 10.0, 1000.0);
    p.bottomHeight = std::clamp(p.bottomHeight, 0.0, 60.0);
    return p;
}

namespace {

// Local shapes are built in [-0.5, 0.5]^3 standing along local Z, then fitted.

Mesh plane(int rows, int cols, bool flip) {
    Mesh m;
    for (int j = 0; j <= rows; ++j)
        for (int i = 0; i <= cols; ++i) m.addVertex({-0.5 + (double)i / cols, -0.5 + (double)j / rows, 0});
    auto id = [&](int i, int j) { return j * (cols + 1) + i; };
    for (int j = 0; j < rows; ++j)
        for (int i = 0; i < cols; ++i) {
            const int f = m.addQuad(id(i, j), id(i + 1, j), id(i + 1, j + 1), id(i, j + 1));
            m.faces[f].diag = flip;
        }
    return m;
}

// Coordinates along one axis of a (rounded) box with half size h.
std::vector<double> boxCoords(double h, int segs, double r, int roundSegs) {
    std::vector<double> c;
    if (r <= 0) {
        for (int i = 0; i <= segs; ++i) c.push_back(-h + 2 * h * i / segs);
        return c;
    }
    const double in = h - r;
    for (int k = 0; k < roundSegs; ++k) c.push_back(-in - r * std::cos(kPi / 2 * k / roundSegs));
    for (int i = 0; i <= segs; ++i) c.push_back(-in + 2 * in * i / segs);
    for (int k = roundSegs - 1; k >= 0; --k) c.push_back(in + r * std::cos(kPi / 2 * k / roundSegs));
    // A full rounding (r == h) makes the middle span zero: drop repeated values.
    c.erase(std::unique(c.begin(), c.end(), [](double a, double b) { return std::fabs(a - b) < 1e-12; }), c.end());
    return c;
}

// Box in real units (half sizes hx, hy, hz), optional rounded edges.
Mesh box(const Vec3& half, int sx, int sy, int sz, double round, int roundSegs) {
    const double r = std::min(round, std::min(half.x, std::min(half.y, half.z)));
    const std::vector<double> L[3] = {boxCoords(half.x, sx, r, roundSegs), boxCoords(half.y, sy, r, roundSegs),
                                      boxCoords(half.z, sz, r, roundSegs)};
    const int n[3] = {(int)L[0].size(), (int)L[1].size(), (int)L[2].size()};
    Mesh m;
    std::map<std::tuple<int, int, int>, int> ids;
    auto vid = [&](int i, int j, int k) {
        auto key = std::make_tuple(i, j, k);
        auto it = ids.find(key);
        if (it != ids.end()) return it->second;
        Vec3 p{L[0][i], L[1][j], L[2][k]};
        if (r > 0) {
            Vec3 in = p;
            const double lim[3] = {half.x - r, half.y - r, half.z - r};
            for (int a = 0; a < 3; ++a) in[a] = std::clamp(p[a], -lim[a], lim[a]);
            const Vec3 d = p - in;
            if (length(d) > 1e-12) p = in + normalize(d) * r;
        }
        const int v = m.addVertex(p);
        ids[key] = v;
        return v;
    };
    const int sg = r > 0 ? 1 : 0;
    for (int a = 0; a < 3; ++a) {
        const int b = (a + 1) % 3, c = (a + 2) % 3;
        for (int side = 0; side < 2; ++side) {
            const int fixed = side ? n[a] - 1 : 0;
            for (int j = 0; j + 1 < n[b]; ++j)
                for (int k = 0; k + 1 < n[c]; ++k) {
                    int idx[4][3];
                    const int jj[4] = {j, j + 1, j + 1, j}, kk[4] = {k, k, k + 1, k + 1};
                    for (int q = 0; q < 4; ++q) {
                        idx[q][a] = fixed;
                        idx[q][b] = jj[q];
                        idx[q][c] = kk[q];
                    }
                    int v[4];
                    for (int q = 0; q < 4; ++q) v[q] = vid(idx[q][0], idx[q][1], idx[q][2]);
                    // e_b x e_c = e_a: counter-clockwise from outside on the + side.
                    if (side) m.addQuad(v[0], v[1], v[2], v[3], sg);
                    else m.addQuad(v[3], v[2], v[1], v[0], sg);
                }
        }
    }
    return m;
}

// Rings of points around the local Z axis. Radius 1 (scaled later), at height z.
void ring(Mesh& m, std::vector<int>& out, int sides, double rx, double ry, double z) {
    out.clear();
    for (int i = 0; i < sides; ++i) {
        const double a = 2 * kPi * i / sides;
        out.push_back(m.addVertex({rx * std::cos(a), ry * std::sin(a), z}));
    }
}

// Side band between two rings (lower a, upper b), facing outward (or inward).
void band(Mesh& m, const std::vector<int>& a, const std::vector<int>& b, int sg, bool inward = false) {
    const int n = (int)a.size();
    for (int i = 0; i < n; ++i) {
        const int i1 = (i + 1) % n;
        if (!inward) m.addQuad(a[i], a[i1], b[i1], b[i], sg);
        else m.addQuad(a[i1], a[i], b[i], b[i1], sg);
    }
}

// A flat cap filling a ring at height z, with `rings` rings of quads and a triangle
// fan in the middle. `up`: facing +Z (top) or -Z (bottom).
void cap(Mesh& m, const std::vector<int>& outer, int rings, double z, bool up) {
    const int n = (int)outer.size();
    std::vector<int> prev = outer;
    for (int r = rings - 1; r >= 1; --r) {
        const double f = (double)r / rings;
        std::vector<int> cur;
        for (int i = 0; i < n; ++i) {
            const Vec3 o = m.verts[outer[i]];
            cur.push_back(m.addVertex({o.x * f, o.y * f, z}));
        }
        for (int i = 0; i < n; ++i) {
            const int i1 = (i + 1) % n;
            if (up) m.addQuad(prev[i], prev[i1], cur[i1], cur[i]);
            else m.addQuad(cur[i], cur[i1], prev[i1], prev[i]);
        }
        prev = cur;
    }
    const int c = m.addVertex({0, 0, z});
    for (int i = 0; i < n; ++i) {
        const int i1 = (i + 1) % n;
        if (up) m.addTri(c, prev[i], prev[i1]);
        else m.addTri(c, prev[i1], prev[i]);
    }
}

Mesh cylinder(int sides, int stacks, int capRings) {
    Mesh m;
    std::vector<std::vector<int>> rs(stacks + 1);
    for (int s = 0; s <= stacks; ++s) ring(m, rs[s], sides, 0.5, 0.5, -0.5 + (double)s / stacks);
    for (int s = 0; s < stacks; ++s) band(m, rs[s], rs[s + 1], 1);
    cap(m, rs[0], capRings, -0.5, false);
    cap(m, rs[stacks], capRings, 0.5, true);
    return m;
}

Mesh cone(int sides, int stacks, int capRings) {
    Mesh m;
    std::vector<std::vector<int>> rs(stacks);
    for (int s = 0; s < stacks; ++s) {
        const double f = 1.0 - (double)s / stacks;
        ring(m, rs[s], sides, 0.5 * f, 0.5 * f, -0.5 + (double)s / stacks);
    }
    for (int s = 0; s + 1 < stacks; ++s) band(m, rs[s], rs[s + 1], 1);
    const int apex = m.addVertex({0, 0, 0.5});
    const std::vector<int>& top = rs[stacks - 1];
    for (int i = 0; i < sides; ++i) m.addTri(top[i], top[(i + 1) % sides], apex, 1);
    cap(m, rs[0], capRings, -0.5, false);
    return m;
}

Mesh tube(int sides, int stacks, double inner) {
    Mesh m;
    std::vector<std::vector<int>> outer(stacks + 1), in(stacks + 1);
    for (int s = 0; s <= stacks; ++s) {
        const double z = -0.5 + (double)s / stacks;
        ring(m, outer[s], sides, 0.5, 0.5, z);
        ring(m, in[s], sides, 0.5 * inner, 0.5 * inner, z);
    }
    for (int s = 0; s < stacks; ++s) {
        band(m, outer[s], outer[s + 1], 1);
        band(m, in[s], in[s + 1], 2, true);
    }
    // Top (+Z) and bottom (-Z) rings between inner and outer.
    for (int i = 0; i < sides; ++i) {
        const int i1 = (i + 1) % sides;
        m.addQuad(outer[stacks][i], outer[stacks][i1], in[stacks][i1], in[stacks][i]);
        m.addQuad(outer[0][i1], outer[0][i], in[0][i], in[0][i1]);
    }
    return m;
}

// Torus in the XY plane: `thick` = tube diameter / overall size (local units).
Mesh torus(int sides, int rings, double thick) {
    Mesh m;
    const double minor = thick / 2;
    const double major = 0.5 - minor;
    std::vector<std::vector<int>> id(sides, std::vector<int>(rings));
    for (int i = 0; i < sides; ++i) {
        const double a = 2 * kPi * i / sides;
        for (int j = 0; j < rings; ++j) {
            const double b = 2 * kPi * j / rings;
            const double rr = major + minor * std::cos(b);
            id[i][j] = m.addVertex({rr * std::cos(a), rr * std::sin(a), minor * std::sin(b)});
        }
    }
    for (int i = 0; i < sides; ++i)
        for (int j = 0; j < rings; ++j) {
            const int i1 = (i + 1) % sides, j1 = (j + 1) % rings;
            m.addQuad(id[i][j], id[i1][j], id[i1][j1], id[i][j1], 1);
        }
    return m;
}

Mesh sphere(int sides, int stacks) {
    Mesh m;
    const int south = m.addVertex({0, 0, -0.5});
    std::vector<std::vector<int>> rs(stacks - 1);
    for (int s = 1; s < stacks; ++s) {
        const double th = kPi * s / stacks; // from the south pole
        const double r = 0.5 * std::sin(th), z = -0.5 * std::cos(th);
        ring(m, rs[s - 1], sides, r, r, z);
    }
    const int north = m.addVertex({0, 0, 0.5});
    for (int i = 0; i < sides; ++i) {
        const int i1 = (i + 1) % sides;
        m.addTri(south, rs[0][i1], rs[0][i], 1);
        m.addTri(north, rs[stacks - 2][i], rs[stacks - 2][i1], 1);
    }
    for (int s = 0; s + 1 < stacks - 1; ++s) band(m, rs[s], rs[s + 1], 1);
    return m;
}

Mesh geosphere(int iterations) {
    const double t = (1.0 + std::sqrt(5.0)) / 2.0;
    std::vector<Vec3> v = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t},
                           {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
    std::vector<std::array<int, 3>> f = {{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
                                         {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
                                         {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
                                         {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}};
    for (Vec3& p : v) p = normalize(p);
    for (int it = 0; it < iterations; ++it) {
        std::map<Edge, int> mid;
        auto midpoint = [&](int a, int b) {
            const Edge e = makeEdge(a, b);
            auto found = mid.find(e);
            if (found != mid.end()) return found->second;
            v.push_back(normalize((v[a] + v[b]) / 2));
            return mid[e] = (int)v.size() - 1;
        };
        std::vector<std::array<int, 3>> nf;
        for (const auto& tri : f) {
            const int a = midpoint(tri[0], tri[1]), b = midpoint(tri[1], tri[2]), c = midpoint(tri[2], tri[0]);
            nf.push_back({tri[0], a, c});
            nf.push_back({tri[1], b, a});
            nf.push_back({tri[2], c, b});
            nf.push_back({a, b, c});
        }
        f.swap(nf);
    }
    Mesh m;
    for (const Vec3& p : v) m.addVertex(p * 0.5);
    for (const auto& tri : f) m.addTri(tri[0], tri[1], tri[2], 1);
    return m;
}

// Local (standing along Z) -> world, for the chosen axis. Cyclic permutations keep
// the winding (they are rotations).
Vec3 toWorld(const Vec3& l, int axis) {
    if (axis == 0) return {l.z, l.x, l.y};
    if (axis == 1) return {l.y, l.z, l.x};
    return l;
}
// World box size -> local size.
Vec3 toLocalSize(const Vec3& s, int axis) {
    if (axis == 0) return {s.y, s.z, s.x};
    if (axis == 1) return {s.z, s.x, s.y};
    return s;
}

} // namespace

Mesh makePrimitive(const PrimParams& in) {
    const PrimParams p = clampParams(in);
    // A box is always lined up with the world axes; round shapes stand along p.axis.
    const int axis = p.kind == PrimKind::Box ? 2 : p.axis;
    const Vec3 ls = toLocalSize(p.size, axis);
    Mesh m;
    switch (p.kind) {
    case PrimKind::Plane: m = plane(p.rows, p.cols, p.flipDiagonals); break;
    case PrimKind::Box: m = box(p.size / 2, p.segX, p.segY, p.segZ, p.round, p.roundSegs); break;
    case PrimKind::Cylinder: m = cylinder(p.sides, p.stacks, p.capRings); break;
    case PrimKind::Cone: m = cone(p.sides, p.stacks, p.capRings); break;
    case PrimKind::Tube: m = tube(p.sides, p.stacks, p.inner); break;
    case PrimKind::Torus: m = torus(p.sides, p.rings, std::clamp(ls.z / std::max(ls.x, ls.y), 0.02, 0.95)); break;
    case PrimKind::Sphere: m = sphere(p.sides, p.stacks); break;
    case PrimKind::Geosphere: m = geosphere(p.iterations); break;
    case PrimKind::Rock: m = makeRock(p); break;
    }
    for (Vec3& v : m.verts) {
        Vec3 l = v;
        if (p.kind != PrimKind::Box) { // the box is built in cm already
            l.x *= ls.x;
            l.y *= ls.y;
            // A torus's thickness is a fraction of its overall size (set above).
            l.z *= p.kind == PrimKind::Torus ? std::max(ls.x, ls.y) : ls.z;
            if (p.kind == PrimKind::Plane) l.z = 0;
        }
        v = toWorld(l, axis) + p.centre;
    }
    return m;
}

} // namespace mazz
