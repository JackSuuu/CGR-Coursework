#pragma once
// Minimal 3D vector / point / normal / ray types, mirroring the pbrt naming so
// that the lecture material maps directly onto the code.
#include <cmath>
#include <cstdint>
#include <ostream>

#include "core/math.h"

namespace cgr {

struct Vec3 {
    double x = 0, y = 0, z = 0;

    constexpr Vec3() = default;
    constexpr explicit Vec3(double v) : x(v), y(v), z(v) {}
    constexpr Vec3(double x, double y, double z) : x(x), y(y), z(z) {}

    double operator[](int i) const { return (&x)[i]; }
    double& operator[](int i) { return (&x)[i]; }
};

using Point3 = Vec3;
using Normal = Vec3;

struct Point2 {
    double x = 0, y = 0;
    constexpr Point2() = default;
    constexpr Point2(double x, double y) : x(x), y(y) {}
};

constexpr Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
constexpr Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr Vec3 operator*(Vec3 a, Vec3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
constexpr Vec3 operator/(Vec3 a, Vec3 b) { return {a.x / b.x, a.y / b.y, a.z / b.z}; }
constexpr Vec3 operator*(double s, Vec3 a) { return {s * a.x, s * a.y, s * a.z}; }
constexpr Vec3 operator*(Vec3 a, double s) { return {s * a.x, s * a.y, s * a.z}; }
constexpr Vec3 operator/(Vec3 a, double s) { return {a.x / s, a.y / s, a.z / s}; }
constexpr Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }

inline Vec3& operator+=(Vec3& a, Vec3 b) { a = a + b; return a; }
inline Vec3& operator-=(Vec3& a, Vec3 b) { a = a - b; return a; }
inline Vec3& operator*=(Vec3& a, double s) { a = a * s; return a; }
inline Vec3& operator*=(Vec3& a, Vec3 b) { a = a * b; return a; }
inline Vec3& operator/=(Vec3& a, double s) { a = a / s; return a; }

inline bool operator==(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
inline bool operator!=(Vec3 a, Vec3 b) { return !(a == b); }

constexpr double Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr double AbsDot(Vec3 a, Vec3 b) { return std::fabs(Dot(a, b)); }
constexpr Vec3 Cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double LengthSquared(Vec3 a) { return Dot(a, a); }
inline double Length(Vec3 a) { return std::sqrt(LengthSquared(a)); }
inline double Distance(Vec3 a, Vec3 b) { return Length(a - b); }
inline double DistanceSquared(Vec3 a, Vec3 b) { return LengthSquared(a - b); }
inline Vec3 Normalize(Vec3 a) {
    double len = Length(a);
    return len > 0 ? a / len : a;
}
inline Vec3 Abs(Vec3 a) { return {std::fabs(a.x), std::fabs(a.y), std::fabs(a.z)}; }
inline Vec3 Sqrt(Vec3 a) { return {std::sqrt(a.x), std::sqrt(a.y), std::sqrt(a.z)}; }
inline Vec3 Min(Vec3 a, Vec3 b) {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}
inline Vec3 Max(Vec3 a, Vec3 b) {
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}
inline Vec3 Clamp(Vec3 a, double lo, double hi) {
    return {Clamp(a.x, lo, hi), Clamp(a.y, lo, hi), Clamp(a.z, lo, hi)};
}
inline Vec3 Lerp(double t, Vec3 a, Vec3 b) { return a * (1 - t) + b * t; }
// Maximum of each component, used for building bounding boxes.
inline double MaxComponent(Vec3 a) { return std::max(a.x, std::max(a.y, a.z)); }
inline int MaxComponentDim(Vec3 a) {
    return (a.x > a.y) ? ((a.x > a.z) ? 0 : 2) : ((a.y > a.z) ? 1 : 2);
}
inline Vec3 Permute(Vec3 v, int x, int y, int z) { return {v[x], v[y], v[z]}; }

// Both wo and the returned direction point away from the surface.
inline Vec3 Reflect(Vec3 wo, Normal n) { return -wo + 2.0f * Dot(wo, n) * n; }

// d points into the surface; n is a unit normal opposing d.
// Returns false for total internal reflection. etaI/etaT is the Snell ratio.
inline bool RefractIncident(Vec3 d, Normal n, double etaI, double etaT, Vec3* wt) {
    double eta = etaI / etaT;
    double cosI = Clamp(-Dot(d, n), 0.0, 1.0);
    double sin2T = eta * eta * std::max(0.0, 1.0 - cosI * cosI);
    if (sin2T >= 1.0) return false;
    double cosT = std::sqrt(std::max(0.0, 1.0 - sin2T));
    *wt = Normalize(eta * d + (eta * cosI - cosT) * n);
    return true;
}

// Cosine-weighted hemisphere sample about the +z axis, given u,v in [0,1).
inline Point3 SampleCosineHemisphere(double u, double v) {
    double r = std::sqrt(u);
    double theta = 2.0 * kPi * v;
    double x = r * std::cos(theta);
    double y = r * std::sin(theta);
    double z = std::sqrt(std::max(0.0, 1.0 - u));
    return {x, y, z};
}

//
// Orthonormal basis construction (Duff et al. 2017) around an arbitrary normal.
//
struct Frame {
    Vec3 s, t, n;
    explicit Frame(Normal nn) : n(Normalize(nn)) {
        double sign = std::copysign(1.0, n.z);
        double a = -1.0 / (sign + n.z);
        double b = n.x * n.y * a;
        s = Vec3(1.0 + sign * n.x * n.x * a, sign * b, -sign * n.x);
        t = Vec3(b, sign + n.y * n.y * a, -n.y);
    }
    explicit Frame(const Vec3& vx, const Vec3& vy, const Vec3& vz)
        : s(vx), t(vy), n(vz) {}
    Vec3 ToLocal(Vec3 v) const { return {Dot(v, s), Dot(v, t), Dot(v, n)}; }
    Vec3 FromLocal(Vec3 v) const { return s * v.x + t * v.y + n * v.z; }
};

//
// Ray: origin + direction, with the p(t) = o + t*d convention used throughout.
//
struct Ray {
    Point3 o;
    Vec3 d;
    double tMax = kInf;

    Ray() = default;
    Ray(Point3 o, Vec3 d, double tMax = kInf) : o(o), d(d), tMax(tMax) {}
    Point3 operator()(double t) const { return o + d * t; }
    Point3 operator[](int i) const { return i == 0 ? o : d; }
};

inline std::ostream& operator<<(std::ostream& os, Vec3 v) {
    return os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
}

}  // namespace cgr
