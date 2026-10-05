#include "scene/light.h"

#include <cmath>

#include "core/texture.h"

namespace cgr {

namespace {
// Nearest positive root of |o + t d - c| = r.
double IntersectSphereRay(Point3 o, Vec3 d, double r) {
    double b = 2.0 * Dot(o, d);
    double c = LengthSquared(o) - r * r;
    double disc = b * b - 4.0 * c;
    if (disc < 0) return -1;
    double s = std::sqrt(disc);
    double t0 = (-b - s) * 0.5, t1 = (-b + s) * 0.5;
    if (t1 > 1e-9) return t0 > 1e-9 ? t0 : t1;
    return -1;
}
}  // namespace

//
// DistantLight: infinite area light described only by a direction.
//
Color DistantLight::L(Point3 p, Normal n, Vec3 w) const {
    (void)p;
    (void)n;
    // The shape spans the whole sphere, so the solid angle subtended by every
    // direction is 1 sr; radiance == intensity.
    if (Dot(w, direction) <= 0) return Color(0);
    return intensity * emission;
}

//
// AreaLight: an emissive rectangle (default), disc or sphere in local space.
//
Color AreaLight::L(Point3 p, Normal n, Vec3 w) const {
    // Radiance leaving the light towards w. n is the (possibly flipped) normal
    // at p facing the shading point.
    (void)p;
    if (Dot(n, w) <= 0) return Color(0);
    return emission * scale;
}

Point3 AreaLight::SamplePoint(Point2 uv, Normal* ns) const {
    double u = uv.x, v = uv.y;
    switch (shape) {
        case Shape::Rectangle: {
            // Local x in [-1,1], y in [-1,1], z = 0, emitting towards +z.
            Point3 lp(2 * u - 1, 2 * v - 1, 0);
            if (ns) *ns = Normal(0, 0, 1);
            return objectToWorld.TransformPoint(lp);
        }
        case Shape::Disk: {
            double r = radius * std::sqrt(u);
            double theta = 2.0 * kPi * v;
            Point3 lp(r * std::cos(theta), r * std::sin(theta), 0);
            if (ns) *ns = Normal(0, 0, 1);
            return objectToWorld.TransformPoint(lp);
        }
        case Shape::Sphere: {
            double z = 1.0 - 2.0 * u;
            double r = std::sqrt(std::max(0.0, 1.0 - z * z));
            double theta = 2.0 * kPi * v;
            Point3 lp(r * std::cos(theta), r * std::sin(theta), z);
            if (ns) *ns = Normalize(lp);
            return objectToWorld.TransformPoint(lp);
        }
    }
    if (ns) *ns = Normal(0, 0, 1);
    return objectToWorld.TransformPoint(Point3(0, 0, 0));
}

Point3 AreaLight::SampleLe(Point3 ref, Point2 uvm, double* dist2, Normal* ns,
                            Vec3* wi) const {
    Point3 p = SamplePoint(uvm, ns);
    Vec3 d = p - ref;
    double d2 = LengthSquared(d);
    *dist2 = d2;
    *wi = d / std::sqrt(std::max(d2, 1e-30));
    return p;
}

double AreaLight::Pdf(Point3 ref, Vec3 wi) const {
    // pbrt's power heuristic for area lights: pick the light with prob 1/N and
    // convert the area pdf to solid angle.
    if (LengthSquared(wi) <= 0) return 0;
    Ray r(ref, Normalize(wi));
    // Intersect with the light's own plane/sphere by sampling proximity: we
    // reuse SampleLe semantics via a closest-point test on the local geometry.
    double cosTheta = 0;
    switch (shape) {
        case Shape::Rectangle: {
            Ray lo = worldToObject.TransformRay(r);
            if (std::fabs(lo.d.z) < 1e-12) return 0;
            double t = -lo.o.z / lo.d.z;
            if (t <= 0) return 0;
            Point3 hit = lo(t);
            if (std::fabs(hit.x) > 1 || std::fabs(hit.y) > 1) return 0;
            cosTheta = std::fabs(lo.d.z);
            break;
        }
        case Shape::Disk: {
            Ray lo = worldToObject.TransformRay(r);
            if (std::fabs(lo.d.z) < 1e-12) return 0;
            double t = -lo.o.z / lo.d.z;
            if (t <= 0) return 0;
            Point3 hit = lo(t);
            if (LengthSquared(Vec3(hit.x, hit.y, 0)) > radius * radius) return 0;
            cosTheta = std::fabs(lo.d.z);
            break;
        }
        case Shape::Sphere: {
            Ray lo = worldToObject.TransformRay(r);
            double t = IntersectSphereRay(lo.o, lo.d, radius);
            if (t <= 0) return 0;
            cosTheta = std::fabs(Dot(lo.d, Normalize(lo(t))));
            break;
        }
    }
    if (cosTheta < 1e-9) return 0;
    double A = Area();
    double dist2 = LengthSquared(SamplePoint(Point2(0.5, 0.5), nullptr) - ref);
    // pdf_area -> pdf_solid_angle
    double pdf = dist2 / (cosTheta * A);
    if (!std::isfinite(pdf) || pdf <= 0) return 0;
    return pdf;
}

double AreaLight::Area() const {
    switch (shape) {
        case Shape::Rectangle: {
            // Local extent is 2 x 2 before any scale in objectToWorld.
            Vec3 x = objectToWorld.TransformVector(Vec3(1, 0, 0));
            Vec3 y = objectToWorld.TransformVector(Vec3(0, 1, 0));
            return 4.0 * Length(Cross(x, y));
        }
        case Shape::Disk: {
            Vec3 x = objectToWorld.TransformVector(Vec3(1, 0, 0));
            return kPi * radius * radius * Length(x) * Length(x);
        }
        case Shape::Sphere:
            return 4.0 * kPi * radius * radius;
    }
    return 0;
}

}  // namespace cgr
