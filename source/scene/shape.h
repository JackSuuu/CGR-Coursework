#pragma once
// Analytic and polygonal primitives with UV generation.
// Transforms are applied to rays rather than to geometry, matching pbrt, so a
// single shape instance can be reused inside a triangle mesh loop.
#include <memory>
#include <vector>

#include "core/aabb.h"
#include "core/color.h"
#include "core/transform.h"
#include "core/vec.h"

namespace cgr {

class Texture2D;
class Material;

struct SurfaceInteraction {
    Point3 p;
    Normal n;        // shading normal
    Normal ng;       // geometric normal
    Point2 uv = {0, 0};
    const Material* material = nullptr;
    double area = 0;
    bool valid = false;
};
class Shape {
  public:
    virtual ~Shape() = default;

    virtual AABB GetBounds() const = 0;
    // Returns true and fills hit when the ray hits within [0, tMax).
    virtual bool Intersect(const Ray& r, double tMax, double* tHit, SurfaceInteraction* hit) const = 0;
    // Same maths, but no hit record is materialised (used for shadow rays).
    virtual bool IntersectRay(const Ray& r, double tMax, double* tHit) const = 0;

    virtual double Area() const = 0;
    virtual Point3 SamplePoint(double u1, double u2, Normal* ns, double* pdf) const = 0;
    virtual double Pdf(const Point3& p) const {
        (void)p;
        return 0;
    }

    // UV parameterisation used for texturing.
    virtual Point2 UV(const Point3& p, const Normal& n) const {
        (void)p;
        (void)n;
        return Point2(0, 0);
    }
    virtual Normal NormalAt(const Point3& p) const = 0;
    virtual bool IsEmissive() const { return isLightEmitter; }

    const Transform& ToWorld() const { return objectToWorld; }
    const Transform& ToObject() const { return worldToObject; }
    void SetTransform(const Transform& t) {
        objectToWorld = t;
        worldToObject = t.GetInverse();
    }

    // The material resolved by the scene reader; shapes are geometry-only.
    Material* material = nullptr;
    // Set by the reader when a shape carries emission, so the DRT can sample
    // directly on emissive geometry.
    bool isLightEmitter = false;
    Color emission = Color(0);

    virtual std::string TypeName() const = 0;

  protected:
    // Object -> world, and its inverse (cached because ToWorld() returns a
    // reference and GetInverse() is a 4x4 solve).
    Transform worldToObject;
    Transform objectToWorld;
    // Offset used to avoid self-intersection at the hit point; the spec asks
    // specifically for correct handling of this.
    double rayEpsilon = 1e-4;
};

using ShapePtr = std::shared_ptr<Shape>;

class Sphere : public Shape {
  public:
    Sphere() = default;
    explicit Sphere(double radius) : radius(radius) {}

    AABB GetBounds() const override;
    bool Intersect(const Ray& r, double tMax, double* tHit, SurfaceInteraction* hit) const override;
    bool IntersectRay(const Ray& r, double tMax, double* tHit) const override;
    double Area() const override { return 4.0 * kPi * radius * radius; }
    Point3 SamplePoint(double u1, double u2, Normal* ns, double* pdf) const override;
    double Pdf(const Point3& p) const override;
    Point2 UV(const Point3& p, const Normal& n) const override;
    Normal NormalAt(const Point3& p) const override;
    std::string TypeName() const override { return "sphere"; }

    double radius = 1.0;
    // Shading normal override (pbrt's Sphere::Radius + eta handling).
    bool insideIsNegative = false;
};

class Plane : public Shape {
  public:
    Plane() = default;

    AABB GetBounds() const override;
    bool Intersect(const Ray& r, double tMax, double* tHit, SurfaceInteraction* hit) const override;
    bool IntersectRay(const Ray& r, double tMax, double* tHit) const override;
    double Area() const override { return 0; }  // unbounded
    Point3 SamplePoint(double u1, double u2, Normal* ns, double* pdf) const override;
    double Pdf(const Point3& p) const override {
        (void)p;
        return 0;
    }
    Point2 UV(const Point3& p, const Normal& n) const override;
    Normal NormalAt(const Point3& p) const override;
    std::string TypeName() const override { return "plane"; }
};

class Triangle : public Shape {
  public:
    Triangle() = default;
    Triangle(const Point3& p0, const Point3& p1, const Point3& p2)
        : p{p0, p1, p2} {
        e1 = p1 - p0;
        e2 = p2 - p0;
        n = Normalize(Cross(e1, e2));
    }

    AABB GetBounds() const override;
    bool Intersect(const Ray& r, double tMax, double* tHit, SurfaceInteraction* hit) const override;
    bool IntersectRay(const Ray& r, double tMax, double* tHit) const override;
    double Area() const override { return 0.5 * Length(Cross(e1, e2)); }
    Point3 SamplePoint(double u1, double u2, Normal* ns, double* pdf) const override;
    double Pdf(const Point3& p) const override {
        (void)p;
        return 0;
    }
    Point2 UV(const Point3& pt, const Normal& nn) const override;
    Normal NormalAt(const Point3& pt) const override;
    std::string TypeName() const override { return "triangle"; }

    Point3 p[3];
    Point2 uv[3] = {Point2(0, 0), Point2(1, 0), Point2(0, 1)};
    Normal sn[3];  // per-vertex shading normals (for smooth meshes)
    Vec3 e1, e2;
    Normal n;
    bool hasVertexNormals = false;
};

}  // namespace cgr
