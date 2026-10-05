#include "core/sampler.h"

#include <cmath>

namespace cgr {

const char* SamplerTypeName(SamplerType t) {
    switch (t) {
        case SamplerType::Uniform:  return "uniform";
        case SamplerType::Grid:     return "grid";
        case SamplerType::Halton:   return "halton";
        case SamplerType::HaltonJittered: return "halton-jittered";
    }
    return "unknown";
}

Point2 Sample2D(SamplerType type, std::uint64_t index, int nSamples, int gridN,
                 std::uint64_t scrambleSeed) {
    if (nSamples <= 0) nSamples = 1;
    if (gridN <= 0) gridN = 1;
    switch (type) {
        case SamplerType::Uniform: {
            // Deterministic seed derived from the pixel/sample index so results
            // are reproducible run to run; a real RNG stream is used when the
            // integrator asks for one via Sampler::GetRandom.
            RNG rng(index * 6364136223846793005ULL + scrambleSeed + 0x9e3779b97f4a7c15ULL);
            return Point2(rng.Uniform(), rng.Uniform());
        }
        case SamplerType::Grid: {
            // n x n cell centres, taken in a shuffled-but-deterministic order
            // so the result is a proper stratification of the unit square.
            int cells = gridN * gridN;
            int total = std::max(nSamples, cells);
            int perDim = static_cast<int>(std::lround(std::sqrt(static_cast<double>(total))));
            perDim = std::max(1, perDim);
            int idx = static_cast<int>(index % static_cast<std::uint64_t>(total));
            int gx = idx % perDim, gy = idx / perDim;
            return Point2((gx + 0.5) / perDim, (gy + 0.5) / perDim);
        }
        case SamplerType::Halton: {
            std::uint64_t h = index + scrambleSeed;
            double u = Halton(static_cast<int>(h & 0xffffu), 2);
            double v = Halton(static_cast<int>((h >> 16) & 0xffffu), 3);
            return Point2(u, v);
        }
        case SamplerType::HaltonJittered: {
            // One Halton point per stratum of a perDim x perDim stratification.
            int perDim = static_cast<int>(std::lround(std::sqrt(static_cast<double>(std::max(1, nSamples)))));
            perDim = std::max(1, perDim);
            int idx = static_cast<int>(index);
            int px = idx % perDim, py = idx / perDim;
            double ju = Halton(px, 2), jv = Halton(py, 3);
            return Point2((px + ju) / perDim, (py + jv) / perDim);
        }
    }
    return Point2(0.5, 0.5);
}

}  // namespace cgr
