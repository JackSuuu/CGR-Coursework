#pragma once
// Scalar math utilities. C++ standard library only.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace cgr {

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kInvPi = 0.31830988618379067154;
static constexpr double kInf = std::numeric_limits<double>::infinity();

inline constexpr double Pi() { return kPi; }
inline constexpr double InvPi() { return kInvPi; }

inline double Radians(double deg) { return deg * kPi / 180.0; }
inline double Degrees(double rad) { return rad * 180.0 / kPi; }

template <typename T>
inline constexpr T Clamp(T v, T lo, T hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

template <typename T>
inline constexpr T Mix(T a, T b, double t) {
    return static_cast<T>(a * (1.0 - t) + b * t);
}

template <typename T>
inline constexpr T Sqr(T v) {
    return v * v;
}

inline double Power(double x, double y) { return std::pow(x, y); }
inline double Exp(double x) { return std::exp(x); }
inline double Sqrt(double x) { return std::sqrt(x); }
inline double Abs(double x) { return std::fabs(x); }
inline double Sin(double x) { return std::sin(x); }
inline double Cos(double x) { return std::cos(x); }
inline double Tan(double x) { return std::tan(x); }
inline double Atan2(double y, double x) { return std::atan2(y, x); }
inline double Floor(double x) { return std::floor(x); }

// Smootherstep-style smooth minimum/maximum used for soft union-like blends.
inline double SmoothStep(double edge0, double edge1, double x) {
    double t = Clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// Return 0 when x < 0, otherwise -log(1 - x) clamped away from the singularity.
inline double SafeLog1p(double x) { return std::log1p(x); }

//
// Sampling
//
inline double PowerHeuristic(double a, double b) { return a * a / (a * a + b * b); }

//
// Intersect a ray with an axis-aligned box slab interval. Returns true if the
// ray overlaps the box within [t0, t1]. Writes the near/far hit distances.
//
inline bool SlabTest(const double o[3], const double d[3], const double bmin[3],
                     const double bmax[3], double t0, double t1, double* tNear,
                     double* tFar) {
    for (int i = 0; i < 3; ++i) {
        double invD = 1.0 / d[i];
        double tn = (bmin[i] - o[i]) * invD;
        double tf = (bmax[i] - o[i]) * invD;
        if (tn > tf) std::swap(tn, tf);
        t0 = tn > t0 ? tn : t0;
        t1 = tf < t1 ? tf : t1;
        if (t0 > t1) return false;
    }
    *tNear = t0;
    *tFar = t1;
    return true;
}

}  // namespace cgr
