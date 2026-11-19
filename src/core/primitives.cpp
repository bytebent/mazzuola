#include "core/primitives.hpp"
#include "core/numbers.hpp"
#include "core/transform.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <map>

namespace mazz {

const char* primKindName(PrimKind k) {
    static const char* n[] = {"Box", "Plane", "Cylinder", "Cone", "Tube", "Torus", "Sphere", "Geosphere"};
    return n[(int)k];
}

bool primKindFromName(const std::string& s, PrimKind& out) {
    for (int i = 0; i < kPrimKindCount; ++i) {
        const std::string n = primKindName((PrimKind)i);
        if (n.size() != s.size()) continue;
        bool eq = true;
        for (size_t k = 0; k < n.size(); ++k)
            if (std::tolower((unsigned char)n[k]) != std::tolower((unsigned char)s[k])) eq = false;
        if (eq) {
            out = (PrimKind)i;
            return true;
        }
    }
    return false;
}

PrimParams clampParams(const PrimParams& in) {
    PrimParams p = in;
    p.segX = std::clamp(p.segX, 1, 32);
    p.segY = std::clamp(p.segY, 1, 32);
    p.segZ = std::clamp(p.segZ, 1, 32);
    p.rows = std::clamp(p.rows, 1, 64);
    p.cols = std::clamp(p.cols, 1, 64);
    p.sides = std::clamp(p.sides, 3, 64);
    p.segments = std::clamp(p.segments, 1, 64);
    p.capRings = std::clamp(p.capRings, 1, 16);
    p.inner = std::clamp(p.inner, 5.0, 95.0);
    p.tubeSides = std::clamp(p.tubeSides, 3, 64);
    p.tubeWidth = std::clamp(p.tubeWidth, 5.0, 95.0);
    p.stacks = std::clamp(p.stacks, 2, 64);
    p.iterations = std::clamp(p.iterations, 0, 4);
    return p;
}

namespace {

// Shapes are built in local coordinates: u, v across (-1..1), w along the axis (0..1),
// counter-clockwise seen from outside in that (u, v, w) frame, which is right-handed
// for every axis (axis, axis+1, axis+2 in turn).
struct Builder {
    Mesh m;
    int point(double u, double v, double w) {
        m.verts.push_back({u, v, w});
        return (int)m.verts.size() - 1;
    }
    void face(std::initializer_list<int> idx, int sg) {
        Face f;
        f.n = (int)idx.size();
        int k = 0;
        for (int i : idx) f.v[k++] = i;
        f.sg = sg;
        m.faces.push_back(f);
    }
};

// Polygon corners: N around, flats facing the axes when N is a multiple of 4.
double cornerAngle(int i, int n) { return 2 * kPi * i / n + ((n % 4 == 0) ? kPi / n : 0); }

// A disc of rings at height w; ring r has radius r/rings. Returns the outer ring (shared
// with the side wall). `up` = the cap faces +w (top) or -w (bottom).
std::vector<int> cap(Builder& b, int n, int rings, double w, bool up, const std::vector<int>* outer) {
    std::vector<std::vector<int>> ring(rings + 1);
    for (int r = 1; r <= rings; ++r) {
        if (r == rings && outer) {
            ring[r] = *outer;
            continue;
        }
        for (int i = 0; i < n; ++i) {
            const double a = cornerAngle(i, n), rad = (double)r / rings;
            ring[r].push_back(b.point(rad * std::cos(a), rad * std::sin(a), w));
        }
    }
    const int centre = b.point(0, 0, w);
    for (int i = 0; i < n; ++i) {
        const int j = (i + 1) % n;
        if (up) b.face({centre, ring[1][i], ring[1][j]}, 0);
        else b.face({centre, ring[1][j], ring[1][i]}, 0);
        for (int r = 1; r < rings; ++r) {
            if (up) b.face({ring[r][i], ring[r + 1][i], ring[r + 1][j], ring[r][j]}, 0);
            else b.face({ring[r][i], ring[r][j], ring[r + 1][j], ring[r + 1][i]}, 0);
        }
    }
    return ring[rings];
}

Mesh cylinder(const PrimParams& p, double topScale) {
    // topScale 1 = cylinder; 0 = cone (apex).
    Builder b;
    const int n = p.sides, s = p.segments;
    std::vector<std::vector<int>> rows;
    for (int k = 0; k <= s; ++k) {
        const double w = (double)k / s;
        const double rad = 1 + (topScale - 1) * w;
        if (rad <= 1e-12) break; // the apex is added below
        std::vector<int> row;
        for (int i = 0; i < n; ++i) {
            const double a = cornerAngle(i, n);
            row.push_back(b.point(rad * std::cos(a), rad * std::sin(a), w));
        }
        rows.push_back(row);
    }
    for (size_t k = 0; k + 1 < rows.size(); ++k)
        for (int i = 0; i < n; ++i) {
            const int j = (i + 1) % n;
            b.face({rows[k][i], rows[k][j], rows[k + 1][j], rows[k + 1][i]}, 1);
        }
    cap(b, n, p.capRings, 0, false, &rows.front());
    if (topScale > 0) {
        cap(b, n, p.capRings, 1, true, &rows.back());
    } else {
        const int apex = b.point(0, 0, 1);
        for (int i = 0; i < n; ++i) b.face({rows.back()[i], rows.back()[(i + 1) % n], apex}, 1);
    }
    return b.m;
}

Mesh tube(const PrimParams& p) {
    Builder b;
    const int n = p.sides, s = p.segments;
    const double in = p.inner / 100.0;
    std::vector<std::vector<int>> out(s + 1), inn(s + 1);
    for (int k = 0; k <= s; ++k)
        for (int i = 0; i < n; ++i) {
            const double a = cornerAngle(i, n), w = (double)k / s;
            out[k].push_back(b.point(std::cos(a), std::sin(a), w));
            inn[k].push_back(b.point(in * std::cos(a), in * std::sin(a), w));
        }
    for (int k = 0; k < s; ++k)
        for (int i = 0; i < n; ++i) {
            const int j = (i + 1) % n;
            b.face({out[k][i], out[k][j], out[k + 1][j], out[k + 1][i]}, 1);
            b.face({inn[k][i], inn[k + 1][i], inn[k + 1][j], inn[k][j]}, 2);
        }
    for (int i = 0; i < n; ++i) {
        const int j = (i + 1) % n;
        b.face({out[0][i], inn[0][i], inn[0][j], out[0][j]}, 0); // bottom ring
        b.face({out[s][i], out[s][j], inn[s][j], inn[s][i]}, 0); // top ring
    }
    return b.m;
}

Mesh torus(const PrimParams& p) {
    Builder b;
    const int n = p.sides, m = p.tubeSides;
    // Across the ring: radius R + r = 1, the tube r wide (a fraction of R + r). Along the
    // axis the tube is a circle that the box then stretches to its height.
    const double r = 0.5 * p.tubeWidth / 100.0;
    const double R = 1 - r;
    std::vector<std::vector<int>> id(n, std::vector<int>(m));
    for (int i = 0; i < n; ++i) {
        const double a = cornerAngle(i, n);
        for (int j = 0; j < m; ++j) {
            const double t = 2 * kPi * j / m;
            const double rr = R + r * std::cos(t);
            id[i][j] = b.point(rr * std::cos(a), rr * std::sin(a), 0.5 + 0.5 * std::sin(t));
        }
    }
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < m; ++j) {
            const int i1 = (i + 1) % n, j1 = (j + 1) % m;
            b.face({id[i][j], id[i1][j], id[i1][j1], id[i][j1]}, 1);
        }
    return b.m;
}

Mesh sphere(const PrimParams& p) {
    Builder b;
    const int n = p.sides, k = p.stacks;
    const int south = b.point(0, 0, 0);
    std::vector<std::vector<int>> rings;
    for (int s = 1; s < k; ++s) {
        const double th = kPi * s / k; // from the south pole
        std::vector<int> ring;
        for (int i = 0; i < n; ++i) {
            const double a = cornerAngle(i, n);
            ring.push_back(b.point(std::sin(th) * std::cos(a), std::sin(th) * std::sin(a), 0.5 - 0.5 * std::cos(th)));
        }
        rings.push_back(ring);
    }
    const int north = b.point(0, 0, 1);
    for (int i = 0; i < n; ++i) {
        const int j = (i + 1) % n;
        b.face({south, rings.front()[j], rings.front()[i]}, 1);
        b.face({north, rings.back()[i], rings.back()[j]}, 1);
        for (size_t s = 0; s + 1 < rings.size(); ++s) b.face({rings[s][i], rings[s][j], rings[s + 1][j], rings[s + 1][i]}, 1);
    }
    return b.m;
}

Mesh geosphere(const PrimParams& p) {
    // Icosahedron, each triangle split in four per iteration, pushed onto the sphere.
    const double t = (1 + std::sqrt(5.0)) / 2;
    std::vector<Vec3> v = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t},
                           {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
    for (Vec3& q : v) q = normalize(q);
    std::vector<std::array<int, 3>> f = {{0, 11, 5}, {0, 5, 1},  {0, 1, 7},   {0, 7, 10}, {0, 10, 11},
                                         {1, 5, 9},  {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
                                         {3, 9, 4},  {3, 4, 2},  {3, 2, 6},   {3, 6, 8},  {3, 8, 9},
                                         {4, 9, 5},  {2, 4, 11}, {6, 2, 10},  {8, 6, 7},  {9, 8, 1}};
    for (int it = 0; it < p.iterations; ++it) {
        std::map<std::pair<int, int>, int> mid;
        auto midpoint = [&](int a, int b) {
            const auto key = std::minmax(a, b);
            auto found = mid.find(key);
            if (found != mid.end()) return found->second;
            v.push_back(normalize((v[a] + v[b]) / 2));
            return mid[key] = (int)v.size() - 1;
        };
        std::vector<std::array<int, 3>> next;
        for (const auto& tri : f) {
            const int a = midpoint(tri[0], tri[1]), b = midpoint(tri[1], tri[2]), c = midpoint(tri[2], tri[0]);
            next.push_back({tri[0], a, c});
            next.push_back({tri[1], b, a});
            next.push_back({tri[2], c, b});
            next.push_back({a, b, c});
        }
        f = next;
    }
    Builder b;
    for (const Vec3& q : v) b.point(q.x, q.y, 0.5 + 0.5 * q.z);
    for (const auto& tri : f) b.face({tri[0], tri[1], tri[2]}, 1);
    return b.m;
}

Mesh plane(const PrimParams& p) {
    Builder b;
    std::vector<std::vector<int>> id(p.rows + 1, std::vector<int>(p.cols + 1));
    for (int r = 0; r <= p.rows; ++r)
        for (int c = 0; c <= p.cols; ++c) id[r][c] = b.point(-1 + 2.0 * c / p.cols, -1 + 2.0 * r / p.rows, 0);
    for (int r = 0; r < p.rows; ++r)
        for (int c = 0; c < p.cols; ++c) {
            b.face({id[r][c], id[r][c + 1], id[r + 1][c + 1], id[r + 1][c]}, 0);
            b.m.faces.back().diag = p.flipDiagonals;
        }
    return b.m;
}

// A box with segments per side, as one closed mesh in local space (shared points found
// by position).
Mesh box(const PrimParams& p, int axis) {
    // Segments are given per world axis; local (u, v, w) = (axis+1, axis+2, axis).
    const int segWorld[3] = {p.segX, p.segY, p.segZ};
    const int su = segWorld[(axis + 1) % 3], sv = segWorld[(axis + 2) % 3], sw = segWorld[axis];
    Builder b;
    std::map<std::array<long, 3>, int> seen;
    auto pt = [&](double u, double v, double w) {
        const std::array<long, 3> key = {std::lround(u * 1e6), std::lround(v * 1e6), std::lround(w * 1e6)};
        auto it = seen.find(key);
        if (it != seen.end()) return it->second;
        return seen[key] = b.point(u, v, w);
    };
    // One side of the box: a grid over two local directions at a fixed third, facing
    // `normalSign` along that third direction.
    auto side = [&](int fixedDir, double fixedVal, int na, int nb, bool flip) {
        const int da = (fixedDir + 1) % 3, db = (fixedDir + 2) % 3;
        auto coord = [&](int dir, int i, int n) { return dir == 2 ? (double)i / n : -1 + 2.0 * i / n; };
        auto at = [&](int i, int j) {
            double c[3];
            c[fixedDir] = fixedVal;
            c[da] = coord(da, i, na);
            c[db] = coord(db, j, nb);
            return pt(c[0], c[1], c[2]);
        };
        for (int i = 0; i < na; ++i)
            for (int j = 0; j < nb; ++j) {
                const int a = at(i, j), b1 = at(i + 1, j), c = at(i + 1, j + 1), d = at(i, j + 1);
                if (flip) b.face({a, d, c, b1}, 0);
                else b.face({a, b1, c, d}, 0);
            }
    };
    const int seg[3] = {su, sv, sw};
    for (int dir = 0; dir < 3; ++dir) {
        const int na = seg[(dir + 1) % 3], nb = seg[(dir + 2) % 3];
        const double lo = dir == 2 ? 0 : -1, hi = 1;
        side(dir, hi, na, nb, false); // (da, db, dir) is right-handed: counter-clockwise from +dir
        side(dir, lo, na, nb, true);
    }
    return b.m;
}

} // namespace

Mesh makePrimitive(const PrimParams& in, const Vec3& mn, const Vec3& mx, int axis) {
    const PrimParams p = clampParams(in);
    axis = std::clamp(axis, 0, 2);
    Mesh m;
    switch (p.kind) {
    case PrimKind::Box: m = box(p, axis); break;
    case PrimKind::Plane: m = plane(p); break;
    case PrimKind::Cylinder: m = cylinder(p, 1); break;
    case PrimKind::Cone: m = cylinder(p, 0); break;
    case PrimKind::Tube: m = tube(p); break;
    case PrimKind::Torus: m = torus(p); break;
    case PrimKind::Sphere: m = sphere(p); break;
    case PrimKind::Geosphere: m = geosphere(p); break;
    }
    // Local (u, v, w) -> world axes (axis+1, axis+2, axis).
    const int au = (axis + 1) % 3, av = (axis + 2) % 3;
    for (Vec3& q : m.verts) {
        Vec3 w;
        w[au] = q.x;
        w[av] = q.y;
        w[axis] = q.z;
        q = w;
    }
    // Fill the box exactly: map the shape's own bounds onto it. A plane lies on the
    // box's floor (its lowest side along the axis).
    Vec3 bmn, bmx;
    m.bounds(bmn, bmx);
    Vec3 tmn = mn, tmx = mx;
    if (p.kind == PrimKind::Plane) tmx[axis] = tmn[axis];
    for (Vec3& q : m.verts) q = clean(stretchInBox(q, bmn, bmx, tmn, tmx));
    return m;
}

} // namespace mazz
