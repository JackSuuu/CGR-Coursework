#pragma once
// Samplers. Module 1 needs stratified/uniform samples for the area-light soft
// shadows; the Halton and regular-grid strategies are included here so the
// Module 2 sampler comparison needs no new infrastructure.
#include <cmath>
#include <cstdint>
#include <vector>

#include "core/math.h"
#include "core/vec.h"

namespace cgr {

// PCG32: small, fast, good statistical quality, header-only.
class RNG {
  public:
    RNG() { Seed(0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL); }
    explicit RNG(std::uint64_t seqIndex) { Seed(0x853c49e6748fea9bULL, seqIndex); }
    void Seed(std::uint64_t initState, std::uint64_t initSeq) {
        state = 0u;
        inc = (initSeq << 1u) | 1u;
        NextUInt();
        state += initState;
        NextUInt();
    }
    std::uint32_t NextUInt() {
        std::uint64_t old = state;
        state = old * 6364136223846793005ULL + inc;
        std::uint32_t xorshifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
        std::uint32_t rot = static_cast<std::uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31));
    }
    // Uniform in [0,1).
    double Uniform() { return NextUInt() * 0x1p-32; }
    double Uniform(double a, double b) { return a + (b - a) * Uniform(); }
    Vec3 UniformVec() { return {Uniform(), Uniform(), Uniform()}; }

  private:
    std::uint64_t state = 0, inc = 0;
};

enum class SamplerType { Uniform, Grid, Halton, HaltonJittered };

// Radical inverse in the given base, used for the Halton sequence.
inline double RadicalInverse(int base, std::uint64_t i) {
    double f = 1.0 / base, r = 0.0;
    while (i > 0) {
        std::uint64_t digit = i % static_cast<std::uint64_t>(base);
        r += f * static_cast<double>(digit);
        i /= static_cast<std::uint64_t>(base);
        f /= base;
    }
    return r;
}

inline double Halton(int index, int base) {
    return RadicalInverse(base, static_cast<std::uint64_t>(index) + 1);
}

// Returns the i-th 2D sample of a named strategy in [0,1)^2.
Point2 Sample2D(SamplerType type, std::uint64_t index, int nSamples,
                 int gridN, std::uint64_t scrambleSeed = 0);

// Per-pixel sample count is often a perfect square, so cache the strategy.
struct Sampler {
    SamplerType type = SamplerType::Grid;
    int nSamples = 4;
    int gridN = 2;          // only used by SamplerType::Grid
    std::uint64_t seed = 0; // scramble seed for the Halton sequence

    Point2 Get(std::uint64_t i) const {
        return Sample2D(type, i, nSamples, gridN, seed);
    }
    // Antialiasing: jitter within the pixel with the same strategy.
    Point2 GetPixelJittered(std::uint64_t i) const { return Get(i); }
};

const char* SamplerTypeName(SamplerType t);

}  // namespace cgr
