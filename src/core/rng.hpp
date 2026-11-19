#pragma once
// Seeded random numbers that give the same results on every platform and compiler
// (std::uniform_*_distribution may differ between standard libraries). PCG32.
#include <cstdint>

namespace mazz {

class Rng {
public:
    explicit Rng(uint64_t seed) { reseed(seed); }
    void reseed(uint64_t seed) {
        state_ = 0;
        inc_ = (seed << 1u) | 1u;
        next();
        state_ += seed ^ 0x853c49e6748fea9bULL;
        next();
    }
    uint32_t next() {
        const uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + inc_;
        const uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
        const uint32_t rot = (uint32_t)(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
    }
    // [0, 1) with 32 bits of precision (exactly representable in a double).
    double unit() { return next() * (1.0 / 4294967296.0); }
    // [a, b)
    double range(double a, double b) { return a + (b - a) * unit(); }
    // [-1, 1)
    double signedUnit() { return unit() * 2.0 - 1.0; }

private:
    uint64_t state_ = 0, inc_ = 1;
};

} // namespace mazz
