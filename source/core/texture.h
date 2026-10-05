#pragma once
// Texture support. Data is loaded from plain PPM (P6/P3) files so no image
// library is needed. Lookup is bilinear with configurable wrap modes.
#include <memory>
#include <string>
#include <vector>

#include "core/color.h"
#include "core/vec.h"

namespace cgr {

enum class TextureWrap { Repeat, Clamp, Black };

class Texture2D {
  public:
    int width = 0, height = 0;
    std::vector<double> data;  // 3 doubles per texel, row 0 = v = 0
    std::string filename;
    TextureWrap wrapS = TextureWrap::Repeat;
    TextureWrap wrapT = TextureWrap::Repeat;
    double uScale = 1.0, vScale = 1.0;

    bool IsValid() const { return width > 0 && height > 0; }
    double& Texel(int x, int y, int c) {
        return data[(static_cast<size_t>(y) * width + x) * 3 + c];
    }
    double Texel(int x, int y, int c) const {
        return data[(static_cast<size_t>(y) * width + x) * 3 + c];
    }
};

using TexturePtr = std::shared_ptr<Texture2D>;

// Loads a PPM file into linear RGB. Returns nullptr and fills `err` on failure
// (the caller logs the error; the spec wants graceful handling, not a crash).
TexturePtr LoadTexturePPM(const std::string& path, std::string* err);

// Bilinear lookup in (u,v) texture space.
Color TextureLookup(const Texture2D& t, double u, double v);
// Nearest-neighbour version, kept for the Module 2 aliasing experiments.
Color TextureLookupNearest(const Texture2D& t, double u, double v);

// A 1x1 white fallback so materials with a declared but missing texture still
// render instead of failing outright.
TexturePtr WhiteTexture();

}  // namespace cgr
