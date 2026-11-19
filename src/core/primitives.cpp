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
    static const char* n[] = {"Box", "Plane", "Cylinder", "Cone", "Tube", "Torus", "Sphere", "Geosphere", "Rock"};
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
    p.roundSegments = std::clamp(p.roundSegments, 0, 3);
    p.roundSize = std::clamp(p.roundSize, 1.0, 90.0);
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
    p.rock = clampRock(p.rock);
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
std::vector<int> cap(Builder& b, int n, int rings, double w, bool up, const std::vector<int>* outer, double outerRad = 1) {
    std::vector<std::vector<int>> ring(rings + 1);
    for (int r = 1; r <= rings; ++r) {
        if (r == rings && outer) {
            ring[r] = *outer;
            continue;
        }
        for (int i = 0; i < n; ++i) {
            const double a = cornerAngle(i, n), rad = outerRad * r / rings;
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

Mesh cylinder(const PrimParams& p, double topScale, double roundRad = 0, double roundH = 0) {
    // topScale 1 = cylinder; 0 = cone (apex). roundRad / roundH: the rounded rim's size
    // across and along the axis in local units (cylinder only; 0 = sharp).
    Builder b;
    const int n = p.sides, s = p.segments;
    const bool rounded = topScale > 0 && p.roundSegments > 0 && roundRad > 0 && roundH > 0;
    // The side's profile, bottom to top: (radius, height).
    std::vector<std::pair<double, double>> profile;
    if (rounded) {
        const int q = p.roundSegments;
        for (int m = 0; m <= q; ++m) { // bottom rim: from the cap's edge round to the wall
            const Vec3 d = normalize(Vec3{(double)m, -(double)(q - m), 0});
            profile.push_back({1 - roundRad + roundRad * d.x, roundH + roundH * d.y});
        }
        for (int k = 1; k < s; ++k) profile.push_back({1, roundH + (1 - 2 * roundH) * k / s});
        for (int m = 0; m <= q; ++m) { // top rim
            const Vec3 d = normalize(Vec3{(double)(q - m), (double)m, 0});
            profile.push_back({1 - roundRad + roundRad * d.x, 1 - roundH + roundH * d.y});
        }
    } else {
        for (int k = 0; k <= s; ++k) profile.push_back({1 + (topScale - 1) * k / s, (double)k / s});
    }
    std::vector<std::vector<int>> rows;
    for (const auto& [rad, w] : profile) {
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
    const double capRad = rounded ? 1 - roundRad : 1;
    cap(b, n, p.capRings, rounded ? 0 : 0, false, &rows.front(), capRad);
    if (topScale > 0) {
        cap(b, n, p.capRings, 1, true, &rows.back(), capRad);
    } else {
        const int apex = b.point(0, 0, 1);
        for (int i = 0; i < n; ++i) b.face({rows.back()[i], rows.back()[(i + 1) % n], apex}, 1);
    }
    return b.m;
}

// A box with rounded edges, built at its real proportions (half sizes hu, hv across,
// height hw along w, from 0): flat faces, a strip of `q` faces along each edge, and a
// patch of q*q triangles on each corner, all rounded with radius `rad` (same in every
// direction).
Mesh roundedBox(double hu, double hv, double hw, double rad, int q) {
    Builder b;
    std::map<std::array<long, 3>, int> seen;
    auto pt = [&](const Vec3& v) {
        const std::array<long, 3> key = {std::lround(v.x * 1e6), std::lround(v.y * 1e6), std::lround(v.z * 1e6)};
        auto it = seen.find(key);
        if (it != seen.end()) return it->second;
        return seen[key] = b.point(v.x, v.y, v.z);
    };
    const Vec3 c{0, 0, hw / 2};
    const Vec3 a{hu - rad, hv - rad, hw / 2 - rad}; // the inner box's half sizes
    // A point on the rounding next to inner corner (sx, sy, sz), in direction mix of
    // the axes by weights (wx, wy, wz) (normalised: an even spread).
    auto round = [&](int sx, int sy, int sz, double wx, double wy, double wz) {
        const Vec3 base = c + Vec3{sx * a.x, sy * a.y, sz * a.z};
        return pt(base + normalize(Vec3{sx * wx, sy * wy, sz * wz}) * rad);
    };
    auto face = [&](std::vector<int> idx, int sg) {
        // Outward: flip if the face looks inward (the shape is convex).
        Mesh tmp;
        Face f;
        f.n = (int)idx.size();
        for (int k = 0; k < f.n; ++k) f.v[k] = idx[k];
        f.sg = sg;
        b.m.faces.push_back(f);
        const int fi = (int)b.m.faces.size() - 1;
        if (dot(faceNormal(b.m, fi), faceCentre(b.m, fi) - c) < 0) std::reverse(b.m.faces[fi].v, b.m.faces[fi].v + f.n);
    };
    const int signs[2] = {-1, 1};
    // Flat faces.
    for (int axis = 0; axis < 3; ++axis)
        for (int s : signs) {
            std::vector<int> idx;
            const int o1 = (axis + 1) % 3, o2 = (axis + 2) % 3;
            const int corner[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
            for (const auto& cr : corner) {
                int sg[3];
                sg[axis] = s;
                sg[o1] = cr[0];
                sg[o2] = cr[1];
                double w[3] = {0, 0, 0};
                w[axis] = 1;
                idx.push_back(round(sg[0], sg[1], sg[2], w[0], w[1], w[2]));
            }
            face(idx, 0);
        }
    // Edge strips: along `along`, between the faces of axes i and j.
    for (int along = 0; along < 3; ++along) {
        const int i = (along + 1) % 3, j = (along + 2) % 3;
        for (int si : signs)
            for (int sj : signs)
                for (int m = 0; m < q; ++m) {
                    std::vector<int> idx;
                    for (int end : {-1, 1})
                        for (int step : {m, m + 1}) {
                            int sg[3];
                            sg[along] = end;
                            sg[i] = si;
                            sg[j] = sj;
                            double w[3] = {0, 0, 0};
                            w[i] = q - step;
                            w[j] = step;
                            idx.push_back(round(sg[0], sg[1], sg[2], w[0], w[1], w[2]));
                        }
                    face({idx[0], idx[1], idx[3], idx[2]}, 1);
                }
    }
    // Corner patches: a triangle grid over the corner's eighth of a sphere.
    for (int sx : signs)
        for (int sy : signs)
            for (int sz : signs) {
                auto at = [&](int ii, int jj) { return round(sx, sy, sz, q - ii - jj, ii, jj); };
                for (int ii = 0; ii < q; ++ii)
                    for (int jj = 0; jj + ii < q; ++jj) {
                        face({at(ii, jj), at(ii + 1, jj), at(ii, jj + 1)}, 1);
                        if (ii + jj + 2 <= q) face({at(ii + 1, jj), at(ii + 1, jj + 1), at(ii, jj + 1)}, 1);
                    }
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
    // Real sizes across (u, v) and along (w) the axis, for roundings the same in every
    // direction.
    const int au0 = (axis + 1) % 3, av0 = (axis + 2) % 3;
    const double su = mx[au0] - mn[au0], sv = mx[av0] - mn[av0], sw = mx[axis] - mn[axis];
    const double roundCm = p.roundSize / 100.0 * 0.5 * std::min({su, sv, sw});
    const bool rounded = p.roundSegments > 0 && roundCm > 0;
    switch (p.kind) {
    case PrimKind::Box:
        m = rounded ? roundedBox(su / 2, sv / 2, sw, roundCm * 0.999, p.roundSegments) : box(p, axis);
        break;
    case PrimKind::Plane: m = plane(p); break;
    case PrimKind::Cylinder:
        m = rounded ? cylinder(p, 1, roundCm / (0.5 * std::min(su, sv)), roundCm / sw) : cylinder(p, 1);
        break;
    case PrimKind::Cone: m = cylinder(p, 0); break;
    case PrimKind::Tube: m = tube(p); break;
    case PrimKind::Torus: m = torus(p); break;
    case PrimKind::Sphere: m = sphere(p); break;
    case PrimKind::Geosphere: m = geosphere(p); break;
    case PrimKind::Rock: m = makeRockLocal(p.rock); break;
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
