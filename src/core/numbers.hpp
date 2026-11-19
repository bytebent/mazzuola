#pragma once
// Clean numbers (ARCHITECTURE §5): every position the model holds is a multiple of
// 1/1000 cm. Commands round with clean() whenever they create or change a point, so the
// model never holds rounding noise (3e-15 is 0, 62.499999999999986 is 62.5).
// formatClean() writes such a value as a plain decimal ("100", "62.5", "-0.125"), and
// reading that text back with strtod gives exactly the same double.
#include "core/math.hpp"
#include <string>

namespace mazz {

constexpr double kCleanSteps = 1000; // steps per cm

double clean(double v);  // nearest multiple of 1/1000; -0 becomes 0
Vec3 clean(const Vec3& v);
bool isClean(double v);

// Cosine and sine of an angle in degrees, exact (0, 1, -1) at multiples of 90°.
void cosSinDeg(double degrees, double& c, double& s);

// Plain decimal, at most 3 digits after the point, trailing zeros dropped, never an
// exponent. The value is cleaned first.
std::string formatClean(double v);

} // namespace mazz
