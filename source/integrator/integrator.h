#pragma once
// Common integrator interface plus the scene-wide intersection helper that both
// integrators use. Module 1 intersects every primitive directly; the
// acceleration structure is introduced in Module 3.
#include <memory>
#include <string>

#include "core/color.h"
#include "core/image.h"
#include "core/vec.h"
#include "scene/scene.h"

namespace cgr {

struct Hit {
    double t = kInf;
    const Shape* shape = nullptr;
    SurfaceInteraction si;
};

// Finds the closest intersection along the ray.
bool IntersectScene(const Scene& scene, const Ray& r, double tMax, Hit* hit);
// Any-hit query for shadow rays.
bool Occluded(const Scene& scene, const Ray& r, double tMax);

class Integrator {
  public:
    virtual ~Integrator() = default;
    // Renders `scene` into `film`. Must be callable repeatedly; the WSRT is
    // deterministic, the DRT consumes its own sample indices.
    virtual Image Render(const Scene& scene) = 0;
    virtual std::string Name() const = 0;
};

std::unique_ptr<Integrator> MakeIntegrator(const std::string& name);

}  // namespace cgr
