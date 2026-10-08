#include "core/sampler.h"

#include <cmath>

namespace cgr {

const char* SamplerTypeName(SamplerType t) {
    switch (t) {
        case SamplerType::Uniform: return "uniform";
    }
    return "unknown";
}

Point2 Sample2D(std::uint64_t index, int nSamples, std::uint64_t scrambleSeed) {
    (void)nSamples; // independent random sampling does not require a grid size
    // One decorrelated PCG32 stream per sample index, so a frame always renders
    // identically no matter in which order the pixels are visited.
    RNG rng(index * 6364136223846793005ULL + scrambleSeed + 0x9e3779b97f4a7c15ULL);
    return Point2(rng.Uniform(), rng.Uniform());
}

}  // namespace cgr
