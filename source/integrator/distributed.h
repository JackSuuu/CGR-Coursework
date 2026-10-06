#pragma once
// Distributed ray tracer (DRT).
//
// Module 1 feature list implemented here:
//   * triangle meshes loaded from OBJ, with per-vertex normals and UVs
//   * textures on mesh surfaces via interpolated UVs
//   * area lights (rectangle / disc / sphere) with soft shadows from stratified
//     sampling of the light surface, at a configurable density
//   * the Phong BRDF with a cosine-weighted diffuse term and an energy-
//     conserving specular lobe
//   * thin-lens defocus blur (depth of field) with configurable aperture radius
//     and focal distance
//   * Reinhard tone mapping of the HDR output, per channel, with a configurable
//     white point
#include "integrator/integrator.h"

namespace cgr {

class DistributedIntegrator : public Integrator {
  public:
    Image Render(const Scene& scene) override;
    std::string Name() const override { return "distributed"; }

    // Accumulates one camera sample. `sampleIndex` drives both the pixel jitter
    // and the light-surface sample, so results are reproducible.
    Color SampleRadiance(const Scene& scene, const Ray& ray, int depth,
                         Color beta, uint64_t sampleIndex) const;

  private:
    // Next sample in a deterministic sequence; the same index is reused for the
    // pixel jitter and the light sample so a run can be reproduced exactly.
    Point2 NextSample(const Scene& scene, uint64_t index) const;
    static double ShadowEps(double dist) { return 1e-4 * std::max(1.0, dist); }
    static Point3 OffsetOrigin(Point3 p, Vec3 d) { return p + d * 1e-4; }
};

}  // namespace cgr
