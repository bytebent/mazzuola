#pragma once
// Small vector / matrix maths. Pure C++, no graphics library.
// Vec3 uses doubles (core precision); Mat4 uses floats (handed to the GPU as is),
// column-major like OpenGL.
#include <cmath>

namespace mazz {

constexpr double kPi = 3.14159265358979323846;
inline double radians(double deg) { return deg * kPi / 180.0; }
inline double degrees(double rad) { return rad * 180.0 / kPi; }

struct Vec3 {
    double x = 0, y = 0, z = 0;
    constexpr Vec3() = default;
    constexpr Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    double& operator[](int i) { return i == 0 ? x : i == 1 ? y : z; }
    double operator[](int i) const { return i == 0 ? x : i == 1 ? y : z; }
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
};

inline double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double length(const Vec3& a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalize(const Vec3& a) {
    const double l = length(a);
    return l > 0 ? a / l : Vec3{};
}
// Unit vector along world axis 0/1/2 (x/y/z).
inline Vec3 axisVec(int axis) { return {axis == 0 ? 1.0 : 0.0, axis == 1 ? 1.0 : 0.0, axis == 2 ? 1.0 : 0.0}; }

struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; // identity
    float& at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.at(row, k) * b.at(k, col);
            r.at(row, col) = s;
        }
    return r;
}

// p transformed by m (w = 1), with perspective divide. Returns w in `wOut` if given.
inline Vec3 transformPoint(const Mat4& m, const Vec3& p, double* wOut = nullptr) {
    const double x = m.at(0, 0) * p.x + m.at(0, 1) * p.y + m.at(0, 2) * p.z + m.at(0, 3);
    const double y = m.at(1, 0) * p.x + m.at(1, 1) * p.y + m.at(1, 2) * p.z + m.at(1, 3);
    const double z = m.at(2, 0) * p.x + m.at(2, 1) * p.y + m.at(2, 2) * p.z + m.at(2, 3);
    const double w = m.at(3, 0) * p.x + m.at(3, 1) * p.y + m.at(3, 2) * p.z + m.at(3, 3);
    if (wOut) *wOut = w;
    return w != 0 ? Vec3{x / w, y / w, z / w} : Vec3{x, y, z};
}

// Camera at `eye` looking along `forward` with `up` (both need not be unit length).
inline Mat4 lookAlong(const Vec3& eye, const Vec3& forward, const Vec3& up) {
    const Vec3 f = normalize(forward);
    const Vec3 r = normalize(cross(f, up));
    const Vec3 u = cross(r, f);
    Mat4 v;
    v.at(0, 0) = (float)r.x; v.at(0, 1) = (float)r.y; v.at(0, 2) = (float)r.z; v.at(0, 3) = (float)-dot(r, eye);
    v.at(1, 0) = (float)u.x; v.at(1, 1) = (float)u.y; v.at(1, 2) = (float)u.z; v.at(1, 3) = (float)-dot(u, eye);
    v.at(2, 0) = (float)-f.x; v.at(2, 1) = (float)-f.y; v.at(2, 2) = (float)-f.z; v.at(2, 3) = (float)dot(f, eye);
    return v;
}

inline Mat4 perspective(double fovyDeg, double aspect, double zNear, double zFar) {
    const double t = 1.0 / std::tan(radians(fovyDeg) / 2);
    Mat4 p;
    p.at(0, 0) = (float)(t / aspect);
    p.at(1, 1) = (float)t;
    p.at(2, 2) = (float)((zFar + zNear) / (zNear - zFar));
    p.at(2, 3) = (float)(2 * zFar * zNear / (zNear - zFar));
    p.at(3, 2) = -1;
    p.at(3, 3) = 0;
    return p;
}

inline Mat4 ortho(double l, double r, double b, double t, double n, double f) {
    Mat4 p;
    p.at(0, 0) = (float)(2 / (r - l));
    p.at(1, 1) = (float)(2 / (t - b));
    p.at(2, 2) = (float)(-2 / (f - n));
    p.at(0, 3) = (float)(-(r + l) / (r - l));
    p.at(1, 3) = (float)(-(t + b) / (t - b));
    p.at(2, 3) = (float)(-(f + n) / (f - n));
    return p;
}

} // namespace mazz
