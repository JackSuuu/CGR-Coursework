#include "scene/light.h"

#include <cmath>

#include "core/texture.h"

namespace cgr {

namespace {
// Nearest positive root of |o + t d - c| = r.
double IntersectSphereRay(Point3 o, Vec3 d, double r) {
    double a = LengthSquared(d);
    double b = 2.0 * Dot(o, d);
    double c = LengthSquared(o) - r * r;
    double disc = b * b - 4.0 * a * c;
    if (disc < 0) return -1;
    double s = std::sqrt(disc);
    double t0 = (-b - s) / (2 * a), t1 = (-b + s) / (2 * a);
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
            if (ns) *ns = objectToWorld.TransformNormal(Normal(0, 0, 1));
            return objectToWorld.TransformPoint(lp);
        }
        case Shape::Disk: {
            double r = radius * std::sqrt(u);
            double theta = 2.0 * kPi * v;
            Point3 lp(r * std::cos(theta), r * std::sin(theta), 0);
            if (ns) *ns = objectToWorld.TransformNormal(Normal(0, 0, 1));
            return objectToWorld.TransformPoint(lp);
        }
        case Shape::Sphere: {
            double z = 1.0 - 2.0 * u;
            double r = std::sqrt(std::max(0.0, 1.0 - z * z));
            double theta = 2.0 * kPi * v;
            Point3 lp = Vec3(r * std::cos(theta), r * std::sin(theta), z) * radius;
            if (ns) *ns = objectToWorld.TransformNormal(Normalize(lp));
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
    if (LengthSquared(wi) <= 0) return 0;
    Ray r(ref, Normalize(wi));
    double t;
    Normal n;
    if (!IntersectRay(r, kInf, &t, &n)) return 0;
    double cosTheta = std::fabs(Dot(n, -r.d));
    double area = Area();
    return cosTheta > 1e-9 && area > 0 ? t * t / (cosTheta * area) : 0;
}

bool AreaLight::IntersectRay(const Ray& ray, double tMax, double* tHit, Normal* ns) const {
    Ray lo = worldToObject.TransformRay(ray);
    double t;
    Normal normal(0, 0, 1);
    switch (shape) {
        case Shape::Rectangle: {
            if (std::fabs(lo.d.z) < 1e-12) return false;
            t = -lo.o.z / lo.d.z;
            Point3 hit = lo(t);
            if (std::fabs(hit.x) > 1 || std::fabs(hit.y) > 1) return false;
            break;
        }
        case Shape::Disk: {
            if (std::fabs(lo.d.z) < 1e-12) return false;
            t = -lo.o.z / lo.d.z;
            Point3 hit = lo(t);
            if (LengthSquared(Vec3(hit.x, hit.y, 0)) > radius * radius) return false;
            break;
        }
        case Shape::Sphere: {
            t = IntersectSphereRay(lo.o, lo.d, radius);
            normal = Normalize(lo(t));
            break;
        }
    }
    if (t <= 1e-4 || t >= tMax) return false;
    *tHit = t;
    if (ns) *ns = objectToWorld.TransformNormal(normal);
    return true;
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
            Vec3 y = objectToWorld.TransformVector(Vec3(0, 1, 0));
            return kPi * radius * radius * Length(Cross(x, y));
        }
        case Shape::Sphere: {
            // Uniform scale only: an ellipsoid needs a non-uniform area PDF.
            double s = Length(objectToWorld.TransformVector(Vec3(1, 0, 0)));
            return 4.0 * kPi * radius * radius * s * s;
        }
    }
    return 0;
}

}  // namespace cgr
