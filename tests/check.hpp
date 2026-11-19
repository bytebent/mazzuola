#pragma once
// Tiny check macro shared by the core test files.
#include "core/math.hpp"
#include <cmath>
#include <cstdio>

extern int gFailures;
extern int gChecks;

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        ++gChecks;                                                                        \
        if (!(cond)) {                                                                    \
            ++gFailures;                                                                  \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                                 \
    } while (0)

inline bool near(double a, double b, double tol = 1e-9) { return std::fabs(a - b) <= tol; }
inline bool nearV(const mazz::Vec3& a, const mazz::Vec3& b, double tol = 1e-9) { return mazz::length(a - b) <= tol; }
