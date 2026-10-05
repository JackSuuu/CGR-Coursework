#pragma once
// Light sources.
//
//   * PointLight  -- WSRT requirement: position + radiant intensity.
//   * AreaLight   -- DRT requirement: an emissive rectangle or disc; shadow
//                    rays are aimed at stratified/uniform/Halton points on the
//                    surface, producing soft shadows.
#include <memory>
#include <string>
#include <vector>

#include "core/color.h"
#include "core/sampler.h"
#include "core/texture.h"
#include "core/transform.h"
#include "core/vec.h"

namespace cgr {

class Light {
  public:
    virtual ~Light() = default;

    // Radiance emitted towards direction `w` (world space) from surface point p.
    virtual Color L(Point3 p, Normal n, Vec3 w) const {
        (void)p;
        (void)n;
        (void)w;
        return Color(0);
    }
    // Point on the light surface, sampled with the given 2D sample.
    virtual Point3 SampleLe(Point3 ref, Point2 uvm, double* dist2, Normal* ns,
                            Vec3* wi) const {
        (void)uvm;
        *dist2 = 0;
        *ns = Normal(0, 0, 0);
        *wi = Vec3(0, 0, 0);
        return ref;
    }
    // pdf of the area sampling distribution w.r.t. solid angle.
    virtual double Pdf(Point3 ref, Vec3 wi) const {
        (void)ref;
        (void)wi;
        return 0;
    }
    virtual double Area() const { return 0; }
    // Sample a point on the emitter and its normal (for direct light sampling
    // from a shading point).
    virtual Point3 SamplePoint(Point2 uv, Normal* ns) const {
        (void)uv;
        if (ns) *ns = Normal(0, 0, 1);
        return Point3(0, 0, 0);
    }
    virtual std::string TypeName() const = 0;
    // Non-zero when the light is sampled explicitly by the DRT (area lights).
    virtual bool IsArea() const { return false; }

    // pbrt-style "blackbody"-free emission colour + optional texture.
    Color emission = Color(1);
    TexturePtr emissionTexture;
    Color scale = Color(1);
    // Local <-> world.
    Transform worldToObject;
    Transform objectToWorld;
    void SetTransform(const Transform& t) {
        objectToWorld = t;
        worldToObject = t.GetInverse();
    }
};

using LightPtr = std::shared_ptr<Light>;

class PointLight : public Light {
  public:
    PointLight() = default;
    explicit PointLight(Point3 p) : position(p) {}

    Color L(Point3, Normal, Vec3) const override { return Color(0); }
    Point3 SamplePoint(Point2 uv, Normal* ns) const override {
        (void)uv;
        if (ns) *ns = Normal(0, 0, 0);
        return position;
    }
    double Pdf(Point3 ref, Vec3 wi) const override {
        (void)ref;
        (void)wi;
        return 1.0;
    }
    std::string TypeName() const override { return "point"; }

    Point3 position = Point3(0, 0, 0);
    Color intensity = Color(1);  // radiant intensity, W/sr
};

class DistantLight : public Light {
  public:
    Color L(Point3, Normal, Vec3 w) const override;
    std::string TypeName() const override { return "distant"; }
    Point3 direction = Point3(0, 0, -1);
    Color intensity = Color(1);
};

class AreaLight : public Light {
  public:
    enum class Shape { Rectangle, Disk, Sphere };

    Color L(Point3 p, Normal n, Vec3 w) const override;
    Point3 SampleLe(Point3 ref, Point2 uvm, double* dist2, Normal* ns,
                    Vec3* wi) const override;
    double Pdf(Point3 ref, Vec3 wi) const override;
    Point3 SamplePoint(Point2 uv, Normal* ns) const override;
    double Area() const override;
    std::string TypeName() const override { return "area"; }
    bool IsArea() const override { return true; }

    Shape shape = Shape::Rectangle;
    // Spherical area light radius (Shape::Sphere).
    double radius = 1.0;
};

}  // namespace cgr
