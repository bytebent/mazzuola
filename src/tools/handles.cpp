#include "tools/handles.hpp"
#include "core/numbers.hpp"
#include <algorithm>
#include <cmath>

namespace mazz {

namespace {
constexpr double kHandle = 4;    // half size of a handle square (px)
constexpr double kHit = 7;       // how close counts as on a handle (px)
constexpr double kRingHit = 7;   // how close counts as on a ring (px)
constexpr int kRingSegments = 72;

// The box's rectangle on screen (2D views).
void screenRect(const ViewGeom& g, const Vec3& mn, const Vec3& mx, double& x0, double& y0, double& x1, double& y1) {
    double ax, ay, bx, by;
    g.project(mn, ax, ay);
    g.project(mx, bx, by);
    x0 = std::min(ax, bx);
    x1 = std::max(ax, bx);
    y0 = std::min(ay, by);
    y1 = std::max(ay, by);
}

// Screen position of handle (su, sv): su -1 left .. +1 right, sv -1 bottom .. +1 top.
void handlePos(double x0, double y0, double x1, double y1, int su, int sv, double& hx, double& hy) {
    hx = su < 0 ? x0 : su > 0 ? x1 : (x0 + x1) / 2;
    hy = sv > 0 ? y0 : sv < 0 ? y1 : (y0 + y1) / 2;
}

bool sideHandlesShown(double x0, double y0, double x1, double y1) { return x1 - x0 > 24 && y1 - y0 > 24; }

double ringRadius(const Vec3& mn, const Vec3& mx) { return std::max(4.0, length(mx - mn) * 0.5 * 1.15); }

// Two unit vectors spanning the plane of ring `axis`.
void ringBasis(int axis, Vec3& a, Vec3& b) {
    a = axisVec((axis + 1) % 3);
    b = axisVec((axis + 2) % 3);
}

double segDist(double ax, double ay, double bx, double by, double x, double y) {
    const double dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy;
    double t = len2 > 0 ? ((x - ax) * dx + (y - ay) * dy) / len2 : 0;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(ax + t * dx - x, ay + t * dy - y);
}
} // namespace

BoxGrab hitBox2D(const ViewGeom& g, const Vec3& mn, const Vec3& mx, double sx, double sy, bool alt) {
    BoxGrab out;
    double x0, y0, x1, y1;
    screenRect(g, mn, mx, x0, y0, x1, y1);
    const bool sides = sideHandlesShown(x0, y0, x1, y1);
    double best = kHit;
    for (int su = -1; su <= 1; ++su)
        for (int sv = -1; sv <= 1; ++sv) {
            if (su == 0 && sv == 0) continue;
            const bool corner = su != 0 && sv != 0;
            if (!corner && (!sides || alt)) continue;
            double hx, hy;
            handlePos(x0, y0, x1, y1, su, sv, hx, hy);
            const double d = std::max(std::fabs(hx - sx), std::fabs(hy - sy));
            if (d <= best) {
                best = d;
                out.kind = (alt && corner) ? BoxGrab::Rotate : BoxGrab::Handle;
                out.su = su;
                out.sv = sv;
            }
        }
    if (out.kind == BoxGrab::None && sx >= x0 && sx <= x1 && sy >= y0 && sy <= y1) out.kind = BoxGrab::Inside;
    return out;
}

BoxGrab hitRings(const ViewGeom& g, const Vec3& mn, const Vec3& mx, double sx, double sy) {
    BoxGrab out;
    const Vec3 c = (mn + mx) / 2;
    const double r = ringRadius(mn, mx);
    double best = kRingHit;
    for (int axis = 0; axis < 3; ++axis) {
        Vec3 a, b;
        ringBasis(axis, a, b);
        double px = 0, py = 0;
        bool havePrev = false;
        for (int i = 0; i <= kRingSegments; ++i) {
            const double t = 2 * kPi * i / kRingSegments;
            double x, y;
            const bool ok = g.project(c + a * (r * std::cos(t)) + b * (r * std::sin(t)), x, y);
            if (ok && havePrev) {
                const double d = segDist(px, py, x, y, sx, sy);
                if (d < best) {
                    best = d;
                    out.kind = BoxGrab::Ring;
                    out.ringAxis = axis;
                }
            }
            px = x;
            py = y;
            havePrev = ok;
        }
    }
    return out;
}

void resizeBox(const ViewGeom& g, const Grid& grid, const Vec3& mn0, const Vec3& mx0, int su, int sv, double du,
               double dv, Vec3& mn, Vec3& mx) {
    mn = mn0;
    mx = mx0;
    const OrthoAxes ax = g.axes();
    const double minSize = grid.snap ? grid.size : 1.0 / kCleanSteps;
    auto push = [&](int axis, int screenSide, double sign, double delta) {
        if (screenSide == 0) return;
        // Screen right/top is the world maximum when the view's axis points that way.
        const bool worldMax = screenSide * sign > 0;
        const double move = delta * sign;
        if (worldMax) {
            double v = grid.snapValue(mx0[axis] + move);
            if (v < mn0[axis] + minSize) v = mn0[axis] + minSize;
            mx[axis] = v;
        } else {
            double v = grid.snapValue(mn0[axis] + move);
            if (v > mx0[axis] - minSize) v = mx0[axis] - minSize;
            mn[axis] = v;
        }
    };
    push(ax.u, su, ax.su, du);
    push(ax.v, sv, ax.sv, dv);
}

Vec3 moveOffset(const ViewGeom& g, const Grid& grid, double du, double dv) {
    const OrthoAxes ax = g.axes();
    Vec3 d;
    d[ax.u] = grid.snapValue(du * ax.su);
    d[ax.v] = grid.snapValue(dv * ax.sv);
    return d;
}

Vec3 viewAxisTowardViewer(const ViewGeom& g) {
    const OrthoAxes ax = g.axes();
    return normalize(ax.forward()) * -1.0;
}

double dragAngle(const ViewGeom& g, const Vec3& centre, double x0, double y0, double x1, double y1, const Vec3& axis) {
    if (g.is3D()) {
        // Where the mouse rays meet the ring's plane: the true angle, whatever the
        // perspective. (Seen edge-on, fall back to the angle on screen below.)
        const Vec3 n = normalize(axis);
        auto onPlane = [&](double sx, double sy, Vec3& hit) {
            Vec3 o, d;
            g.ray(sx, sy, o, d);
            const double den = dot(d, n);
            if (std::fabs(den) < 0.15) return false;
            const double t = dot(centre - o, n) / den;
            if (t <= 0) return false;
            hit = o + d * t - centre;
            return length(hit) > 1e-6;
        };
        Vec3 a, b;
        if (onPlane(x0, y0, a) && onPlane(x1, y1, b)) return degrees(std::atan2(dot(cross(a, b), n), dot(a, b)));
    }
    double cx, cy;
    if (!g.project(centre, cx, cy)) return 0;
    // Screen y grows downward: flip it so angles are counter-clockwise as seen.
    const double a0 = std::atan2(-(y0 - cy), x0 - cx);
    const double a1 = std::atan2(-(y1 - cy), x1 - cx);
    double deg = degrees(a1 - a0);
    while (deg > 180) deg -= 360;
    while (deg < -180) deg += 360;
    // Counter-clockwise on screen is a positive turn about an axis pointing at the viewer.
    Vec3 toViewer;
    if (g.is3D()) toViewer = (g.cam.eye() - centre);
    else toViewer = viewAxisTowardViewer(g);
    return dot(axis, toViewer) >= 0 ? deg : -deg;
}

void drawBox2D(ImDrawList* dl, const ViewGeom& g, const Vec3& mn, const Vec3& mx, ImU32 col, bool handles, bool dashed) {
    double x0, y0, x1, y1;
    screenRect(g, mn, mx, x0, y0, x1, y1);
    const ImVec2 a((float)x0, (float)y0), b((float)x1, (float)y1);
    if (!dashed) {
        dl->AddRect(a, b, col, 0, 0, 1.0f);
    } else {
        auto dashLine = [&](ImVec2 p, ImVec2 q) {
            const float len = std::hypot(q.x - p.x, q.y - p.y);
            const float step = 6;
            for (float t = 0; t < len; t += step * 2) {
                const float t1 = std::min(len, t + step);
                dl->AddLine({p.x + (q.x - p.x) * t / len, p.y + (q.y - p.y) * t / len},
                            {p.x + (q.x - p.x) * t1 / len, p.y + (q.y - p.y) * t1 / len}, col, 1.5f);
            }
        };
        dashLine(a, {b.x, a.y});
        dashLine({b.x, a.y}, b);
        dashLine(b, {a.x, b.y});
        dashLine({a.x, b.y}, a);
    }
    if (!handles) return;
    const bool sides = sideHandlesShown(x0, y0, x1, y1);
    for (int su = -1; su <= 1; ++su)
        for (int sv = -1; sv <= 1; ++sv) {
            if (su == 0 && sv == 0) continue;
            if ((su == 0 || sv == 0) && !sides) continue;
            double hx, hy;
            handlePos(x0, y0, x1, y1, su, sv, hx, hy);
            const ImVec2 p0((float)(hx - kHandle), (float)(hy - kHandle)), p1((float)(hx + kHandle), (float)(hy + kHandle));
            dl->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, 255));
            dl->AddRect(p0, p1, col, 0, 0, 1.5f);
        }
}

void drawBox3D(ImDrawList* dl, const ViewGeom& g, const Vec3& mn, const Vec3& mx, ImU32 col) {
    auto corner = [&](int i) { return Vec3{(i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z}; };
    static const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                                     {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& e : edges) {
        double ax, ay, bx, by;
        if (!g.project(corner(e[0]), ax, ay) || !g.project(corner(e[1]), bx, by)) continue;
        dl->AddLine({(float)ax, (float)ay}, {(float)bx, (float)by}, col, 1.5f);
    }
}

void drawRings(ImDrawList* dl, const ViewGeom& g, const Vec3& mn, const Vec3& mx, int hotAxis) {
    static const ImU32 cols[3] = {IM_COL32(208, 62, 62, 255), IM_COL32(58, 156, 74, 255), IM_COL32(62, 102, 210, 255)};
    const Vec3 c = (mn + mx) / 2;
    const double r = ringRadius(mn, mx);
    for (int axis = 0; axis < 3; ++axis) {
        Vec3 a, b;
        ringBasis(axis, a, b);
        double px = 0, py = 0;
        bool havePrev = false;
        const float width = axis == hotAxis ? 3.0f : 1.5f;
        for (int i = 0; i <= kRingSegments; ++i) {
            const double t = 2 * kPi * i / kRingSegments;
            double x, y;
            const bool ok = g.project(c + a * (r * std::cos(t)) + b * (r * std::sin(t)), x, y);
            if (ok && havePrev) dl->AddLine({(float)px, (float)py}, {(float)x, (float)y}, cols[axis], width);
            px = x;
            py = y;
            havePrev = ok;
        }
    }
}

} // namespace mazz
