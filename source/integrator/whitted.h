#pragma once
// Whitted-style ray tracer (WSRT).
//
// Module 1 feature list implemented here:
//   * ideal mirror reflection, recursive to the maximum ray-tree depth
//   * specular refraction with Snell's law, the Fresnel equations and correct
//     total internal reflection
//   * Blinn-Phong shading with per-material diffuse colour, specular colour and
//     shininess
//   * point lights with configurable position and radiant intensity
//   * shadow rays, offset along the shading normal so nothing self-shadows
//   * UV-mapped textures on spheres, planes and triangles
//   * planes, spheres and individual triangles, each with UV generation
//
// A Whitted tracer by definition has *no* indirect diffuse bounce: at a glossy
// hit point only the direct (light + shadow) contribution is accumulated, and
// recursion happens exclusively on the delta lobes. That is what makes the
// energy-conservation argument and the comparison with the DRT meaningful.
#include "integrator/integrator.h"

namespace cgr {

class WhittedIntegrator : public Integrator {
  public:
    Image Render(const Scene& scene, const BVH* bvh) override;
    std::string Name() const override { return "whitted"; }

    Color Radiance(const Scene& scene, const BVH* bvh, const Ray& ray, int depth,
                   Color throughput) const;

  private:
    // Ray offset for shadow rays. Scaling with the shading-point distance keeps
    // the epsilon meaningful in scenes built at very different scales.
    static double ShadowEps(double dist) { return 1e-4 * std::max(1.0, dist); }
    static Point3 OffsetOrigin(Point3 p, Vec3 d) { return p + d * 1e-4; }
};

}  // namespace cgr
