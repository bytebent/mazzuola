#include "core/numbers.hpp"
#include <cmath>
#include <cstdio>

namespace mazz {

double clean(double v) {
    if (!std::isfinite(v)) return 0;
    const double r = std::round(v * kCleanSteps) / kCleanSteps;
    return r == 0 ? 0.0 : r; // also turns -0 into 0
}

Vec3 clean(const Vec3& v) { return {clean(v.x), clean(v.y), clean(v.z)}; }

bool isClean(double v) { return std::isfinite(v) && clean(v) == v && !(v == 0 && std::signbit(v)); }

void cosSinDeg(double degrees, double& c, double& s) {
    double d = std::fmod(degrees, 360.0);
    if (d < 0) d += 360;
    if (d == 0) { c = 1; s = 0; return; }
    if (d == 90) { c = 0; s = 1; return; }
    if (d == 180) { c = -1; s = 0; return; }
    if (d == 270) { c = 0; s = -1; return; }
    const double r = radians(d);
    c = std::cos(r);
    s = std::sin(r);
}

std::string formatClean(double v) {
    v = clean(v);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.3f", v); // never an exponent
    std::string s = buf;
    if (s.find('.') != std::string::npos) {
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }
    if (s == "-0") s = "0";
    return s;
}

} // namespace mazz
