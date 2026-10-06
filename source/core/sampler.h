#pragma once
// Sampling infrastructure. Module 1 only needs independent uniform samples for
// the area-light soft shadows, so that is all this file provides; the regular
// grid and Halton strategies arrive with the Module 2 sampler comparison.
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

enum class SamplerType { Uniform };

// Returns the index-th 2D sample of the strategy in [0,1)^2.
Point2 Sample2D(std::uint64_t index, int nSamples, std::uint64_t scrambleSeed = 0);

// The per-pixel sample count is fixed for a whole frame, so cache it.
struct Sampler {
    int nSamples = 4;
    std::uint64_t seed = 0;

    Point2 Get(std::uint64_t i) const { return Sample2D(i, nSamples, seed); }
};

const char* SamplerTypeName(SamplerType t);

}  // namespace cgr
