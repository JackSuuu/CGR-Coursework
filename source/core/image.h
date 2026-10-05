#pragma once
// HDR image buffer plus the output writers. PPM (P6) needs no external
// library; PNG is implemented here on top of the standard library's
// <zlib>-free stored-block deflate so it stays dependency-free.
#include <string>
#include <vector>

#include "core/color.h"
#include "core/vec.h"

namespace cgr {

enum class ImageFormat { PPM, PNG };

struct Image {
    int xSize = 0, ySize = 0, nChannels = 3;
    std::vector<double> pixels;  // row 0 is the top scanline

    Image() = default;
    Image(int x, int y, int n = 3) : xSize(x), ySize(y), nChannels(n) {
        pixels.assign(static_cast<size_t>(x) * y * n, 0.0);
    }

    size_t Offset(int x, int y) const {
        return (static_cast<size_t>(y) * xSize + x) * nChannels;
    }
    double& operator()(int x, int y, int c = 0) { return pixels[Offset(x, y) + c]; }
    double operator()(int x, int y, int c = 0) const { return pixels[Offset(x, y) + c]; }

    Color GetPixel(int x, int y) const {
        if (nChannels == 1) return {(*this)(x, y, 0), (*this)(x, y, 0), (*this)(x, y, 0)};
        return Color((*this)(x, y, 0), (*this)(x, y, 1), (*this)(x, y, 2));
    }
    void SetPixel(int x, int y, Color c) {
        for (int i = 0; i < nChannels; ++i) (*this)(x, y, i) = c[i];
    }
};

// Tonemap + sRGB encode + write. Split from the render so the two can be
// tested (and swapped for a Reinhard variant) independently.
Color ToneMapReinhard(Color c, double whitePoint, bool perChannel);
Color ToneMapReinhardLuminance(Color c, double whitePoint);

bool WritePPM(const Image& img, const std::string& path, bool srgbEncode);
bool WritePNG(const Image& img, const std::string& path, bool srgbEncode);
bool ReadPPM(const std::string& path, Image* out, std::string* err);
bool WriteImage(const Image& img, const std::string& path, ImageFormat fmt,
                bool srgbEncode);

// Image statistics used by the Module 3 test-suite comparisons.
struct ImageStats {
    double rmse = 0;       // root-mean-square error against a reference
    double maxAbsErr = 0;
    double meanRef[3] = {0, 0, 0};
    double meanErr[3] = {0, 0, 0};
    double psnr = 0;       // in dB, 0 if rmse == 0
    double meanAbsPct = 0;  // mean |diff| as % of reference magnitude
    long long nDiffering = 0;
    long long nTotal = 0;
};
ImageStats CompareImages(const Image& a, const Image& b);
// Fraction of pixels whose luminance differs by more than `tol`.
double DifferingPixelFraction(const Image& a, const Image& b, double tol = 1.0 / 255.0);

}  // namespace cgr
