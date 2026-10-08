#include "scene/shape.h"

#include <cmath>

#include "core/logger.h"
#include "core/math.h"

namespace cgr {

//
// Sphere
//
AABB Sphere::GetBounds() const {
    Transform t = ToWorld();
    Point3 c = t.TransformPoint(Point3(0, 0, 0));
    // Conservative: scale the radius by the largest axis scale.
    Vec3 s = t.TransformVector(Vec3(1, 0, 0));
    double scale = std::max({Length(s), Length(t.TransformVector(Vec3(0, 1, 0))),
                             Length(t.TransformVector(Vec3(0, 0, 1)))});
    double r = radius * scale;
    return AABB(c - Vec3(r, r, r), c + Vec3(r, r, r));
}

bool Sphere::IntersectRay(const Ray& r, double tMax, double* tHit) const {
    Ray ray = ToObject().TransformRay(r);
    Point3 o = ray.o - Point3(0, 0, 0);
    double a = LengthSquared(ray.d);
    double b = 2.0 * Dot(o, ray.d);
    double c = LengthSquared(o) - radius * radius;
    double disc = b * b - 4 * a * c;
    if (disc < 0) return false;
    double sqrtdisc = std::sqrt(disc);
    double t0 = (-b - sqrtdisc) / (2 * a);
    double t1 = (-b + sqrtdisc) / (2 * a);
    if (t0 > t1) std::swap(t0, t1);
    double eps = rayEpsilon * std::max(1.0, radius);
    if (t1 > eps) {
        double t = t0 > eps ? t0 : t1;
        if (t < tMax && t > 0) {
            *tHit = t;
            return true;
        }
    }
    return false;
}

bool Sphere::Intersect(const Ray& r, double tMax, double* tHit,
                       SurfaceInteraction* hit) const {
    if (!IntersectRay(r, tMax, tHit)) return false;
    // IntersectRay solves in object space, so the ray parameter has to be
    // evaluated there as well before the point is mapped to world space.
    Ray rayObj = ToObject().TransformRay(r);
    Point3 pp = rayObj(*tHit);
    Normal nn = Normalize(pp);
    Transform t = ToWorld();
    hit->p = t.TransformPoint(pp);
    hit->n = t.TransformNormal(nn);
    hit->ng = hit->n;
    hit->uv = UV(pp, nn);
    hit->area = Area();
    hit->valid = true;
    return true;
}

Point3 Sphere::SamplePoint(double u1, double u2, Normal* ns, double* pdf) const {
    // Uniform-over-area sampling: z is uniform in [-1,1], radius ~ sqrt(1-z^2).
    double z = 1.0 - 2.0 * u1;
    double r = std::sqrt(std::max(0.0, 1.0 - z * z));
    double theta = 2.0 * kPi * u2;
    Normal nrm = Normalize(Vec3(r * std::cos(theta), r * std::sin(theta), z));
    if (ns) *ns = nrm;
    if (pdf) *pdf = 1.0 / (4.0 * kPi * radius * radius);
    Transform t = ToWorld();
    return t.TransformPoint(Point3(0, 0, 0)) + t.TransformVector(nrm) * radius;
}

double Sphere::Pdf(const Point3& p) const {
    return p.x == kInf ? 0.0 : 1.0 / (4.0 * kPi * radius * radius);
}

Normal Sphere::NormalAt(const Point3& p) const { return Normalize(p); }

Point2 Sphere::UV(const Point3& p, const Normal& n) const {
    (void)p;
    // Longitude around +z and latitude from +z: equirectangular mapping.
    double phi = std::atan2(n.y, n.x);
    if (phi < 0) phi += 2 * kPi;
    double theta = std::acos(Clamp(n.z, -1.0, 1.0));
    return Point2(phi / (2 * kPi), theta / kPi);
}

//
// Plane. Lies in the local z = 0 plane facing +z.
//
AABB Plane::GetBounds() const {
    // Infinite extent: clamp to a large but finite box so the BVH stays valid.
    static const double kBig = 1e6;
    Transform t = ToWorld();
    Point3 c = t.TransformPoint(Point3(0, 0, 0));
    return AABB(c - Vec3(kBig, kBig, kBig), c + Vec3(kBig, kBig, kBig));
}

bool Plane::IntersectRay(const Ray& r, double tMax, double* tHit) const {
    Ray ray = ToObject().TransformRay(r);
    if (std::fabs(ray.d.z) < 1e-12) return false;
    double t = -ray.o.z / ray.d.z;
    if (t <= rayEpsilon || t >= tMax) return false;
    *tHit = t;
    return true;
}

bool Plane::Intersect(const Ray& r, double tMax, double* tHit,
                      SurfaceInteraction* hit) const {
    if (!IntersectRay(r, tMax, tHit)) return false;
    // tHit comes from the object-space solve, so everything before the
    // objectToWorld mapping has to stay in object space.
    Ray rayObj = ToObject().TransformRay(r);
    Point3 pp = rayObj(*tHit);
    Normal nn(0, 0, 1);
    // Keep the geometric orientation; integrators face-forward the shading
    // normal separately, and dielectrics need ng to distinguish entry/exit.
    Transform t = ToWorld();
    hit->p = t.TransformPoint(pp);
    hit->n = t.TransformNormal(nn);
    hit->ng = hit->n;
    hit->uv = UV(pp, nn);
    hit->area = 0;
    hit->valid = true;
    return true;
}

Point3 Plane::SamplePoint(double u1, double u2, Normal* ns, double* pdf) const {
    // Not area-samplable (infinite), so pick a unit patch in local space.
    Point3 p(2 * u1 - 1, 2 * u2 - 1, 0);
    if (ns) *ns = Normal(0, 0, 1);
    if (pdf) *pdf = 0;
    return ToWorld().TransformPoint(p);
}

Point2 Plane::UV(const Point3& p, const Normal& n) const {
    (void)n;
    return Point2(0.5 * p.x, 0.5 * p.y);
}

Normal Plane::NormalAt(const Point3& p) const {
    (void)p;
    return Normal(0, 0, 1);
}

//
// Triangle. Moller-Trumbore in object space.
//
AABB Triangle::GetBounds() const {
    Transform t = ToWorld();
    AABB b;
    for (int i = 0; i < 3; ++i) b.Expand(t.TransformPoint(p[i]));
    return b.Padded(0.0);
}

bool Triangle::IntersectRay(const Ray& r, double tMax, double* tHit) const {
    Ray ray = ToObject().TransformRay(r);
    Vec3 pv = Cross(ray.d, e2);
    double det = Dot(e1, pv);
    if (std::fabs(det) < 1e-12) return false;
    double invDet = 1.0 / det;
    Vec3 tv = ray.o - p[0];
    double u = Dot(tv, pv) * invDet;
    if (u < -1e-9 || u > 1 + 1e-9) return false;
    Vec3 qv = Cross(tv, e1);
    double v = Dot(ray.d, qv) * invDet;
    if (v < -1e-9 || u + v > 1 + 1e-9) return false;
    double t = Dot(e2, qv) * invDet;
    if (t <= rayEpsilon || t >= tMax) return false;
    *tHit = t;
    return true;
}

bool Triangle::Intersect(const Ray& r, double tMax, double* tHit,
                         SurfaceInteraction* hit) const {
    if (!IntersectRay(r, tMax, tHit)) return false;
    // tHit, p, e1/e2 and sn are all object-space, so the record has to be
    // rebuilt with the object-space ray and mapped afterwards.
    Ray rayObj = ToObject().TransformRay(r);
    Point3 pp = rayObj(*tHit);
    // Barycentric coordinates, recomputed for the record.
    Vec3 tv = rayObj.o - p[0];
    Vec3 pv = Cross(rayObj.d, e2);
    double det = Dot(e1, pv);
    double invDet = 1.0 / det;
    double b1 = Dot(tv, pv) * invDet;
    Vec3 qv = Cross(tv, e1);
    double b2 = Dot(rayObj.d, qv) * invDet;
    double b0 = 1 - b1 - b2;

    Normal ng = n;
    Normal ns = ng;
    if (hasVertexNormals) {
        ns = Normalize(sn[0] * b0 + sn[1] * b1 + sn[2] * b2);
        if (Dot(ns, ng) < 0) ns = -ns;
    }
    Transform t = ToWorld();
    hit->p = t.TransformPoint(pp);
    hit->ng = t.TransformNormal(ng);
    hit->n = t.TransformNormal(ns);
    hit->uv = Point2(uv[0].x * b0 + uv[1].x * b1 + uv[2].x * b2,
                     uv[0].y * b0 + uv[1].y * b1 + uv[2].y * b2);
    hit->area = Area();
    hit->valid = true;
    return true;
}

Point3 Triangle::SamplePoint(double u1, double u2, Normal* ns, double* pdf) const {
    // Uniform barycentric sampling: (1-sqrt(u1), sqrt(u1)(1-u2), sqrt(u1)u2).
    double su = std::sqrt(u1);
    double b0 = 1 - su, b1 = su * (1 - u2), b2 = su * u2;
    if (pdf) *pdf = 1.0 / Area();
    if (ns) *ns = n;
    return p[0] * b0 + p[1] * b1 + p[2] * b2;
}

Point2 Triangle::UV(const Point3& pt, const Normal& nn) const {
    (void)nn;
    // Barycentric interpolation of the stored vertex UVs.
    Vec3 e0 = p[1] - p[0], e1v = p[2] - p[0];
    Vec3 q = pt - p[0];
    double d00 = Dot(e0, e0), d01 = Dot(e0, e1v), d11 = Dot(e1v, e1v);
    double d20 = Dot(q, e0), d21 = Dot(q, e1v);
    double den = d00 * d11 - d01 * d01;
    if (std::fabs(den) < 1e-20) return uv[0];
    double v = (d11 * d20 - d01 * d21) / den;
    double w = (d00 * d21 - d01 * d20) / den;
    double u = 1 - v - w;
    return Point2(uv[0].x * u + uv[1].x * v + uv[2].x * w,
                  uv[0].y * u + uv[1].y * v + uv[2].y * w);
}

Normal Triangle::NormalAt(const Point3& pt) const {
    (void)pt;
    return n;
}

}  // namespace cgr
