#pragma once
// Linear RGB colour type. All shading maths happens in linear space; the
// gamma encode happens once, at image-write time.
#include <cmath>
#include <string>

#include "core/vec.h"

namespace cgr {

struct Color {
    double r = 0, g = 0, b = 0;

    constexpr Color() = default;
    constexpr Color(double v) : r(v), g(v), b(v) {}
    constexpr Color(double r, double g, double b) : r(r), g(g), b(b) {}

    double operator[](int i) const { return (&r)[i]; }
    double& operator[](int i) { return (&r)[i]; }

    bool IsBlack() const { return r == 0 && g == 0 && b == 0; }
};

inline Color operator+(Color a, Color b) { return {a.r + b.r, a.g + b.g, a.b + b.b}; }
inline Color operator-(Color a, Color b) { return {a.r - b.r, a.g - b.g, a.b - b.b}; }
inline Color operator*(Color a, Color b) { return {a.r * b.r, a.g * b.g, a.b * b.b}; }
inline Color operator*(Color a, double s) { return {a.r * s, a.g * s, a.b * s}; }
inline Color operator*(double s, Color a) { return {a.r * s, a.g * s, a.b * s}; }
inline Color operator/(Color a, double s) { return {a.r / s, a.g / s, a.b / s}; }
inline Color operator-(Color a) { return {-a.r, -a.g, -a.b}; }
inline Color& operator+=(Color& a, Color b) { a = a + b; return a; }
inline Color& operator-=(Color& a, Color b) { a = a - b; return a; }
inline Color& operator*=(Color& a, double s) { a = a * s; return a; }
inline Color& operator*=(Color& a, Color b) { a = a * b; return a; }
inline bool operator==(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

inline double Luminance(Color c) { return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b; }
inline Color Saturated(Color c) { return {Luminance(c), Luminance(c), Luminance(c)}; }
inline Color Exp(Color c) { return {std::exp(c.r), std::exp(c.g), std::exp(c.b)}; }
inline Color Sqrt(Color c) { return {std::sqrt(c.r), std::sqrt(c.g), std::sqrt(c.b)}; }
inline Color Pow(Color c, double e) {
    return {std::pow(c[0], e), std::pow(c[1], e), std::pow(c[2], e)};
}
inline Color Min(Color a, Color b) {
    return {std::min(a.r, b.r), std::min(a.g, b.g), std::min(a.b, b.b)};
}
inline Color Max(Color a, Color b) {
    return {std::max(a.r, b.r), std::max(a.g, b.g), std::max(a.b, b.b)};
}
inline Color Clamp(Color c, double lo, double hi) {
    return {Clamp(c.r, lo, hi), Clamp(c.g, lo, hi), Clamp(c.b, lo, hi)};
}
inline Color Lerp(double t, Color a, Color b) { return a * (1 - t) + b * t; }

// Parse the colour forms pbrt accepts: "r g b", "r g b [lum]" in the
// Spectrum/Color syntax, plus named colours used by our own scenes.
Color ParseColorSpec(const std::string& spec, bool* ok = nullptr);
const char* ColorTypeName();

//
// Colour-space conversions used by the image writer and the comparison tools.
//
inline double LinearToSRGB(double v) {
    if (v <= 0.0031308) return 12.92 * v;
    return 1.055 * std::pow(v, 1.0 / 2.4) - 0.055;
}
inline double SRGBToLinear(double v) {
    if (v <= 0.04045) return v / 12.92;
    return std::pow((v + 0.055) / 1.055, 2.4);
}

}  // namespace cgr
