#include "integrator/integrator.h"

#include "integrator/distributed.h"
#include "integrator/whitted.h"

namespace cgr {

bool IntersectScene(const Scene& scene, const Ray& r, double tMax, Hit* hit) {
    double t = tMax;
    int idx = -1;
    // Module 1: linear scan over every primitive, keeping the closest hit.
    {
        for (size_t i = 0; i < scene.shapes.size(); ++i) {
            double th;
            if (scene.shapes[i]->IntersectRay(r, t, &th) && th < t) {
                t = th;
                idx = static_cast<int>(i);
            }
        }
        if (idx < 0) return false;
    }
    hit->t = t;
    hit->shape = scene.shapes[idx].get();
    // Re-run the full intersection to obtain the surface record. The upper bound
    // has to be relaxed slightly: the primitives reject hits with t >= tMax, so
    // passing the distance we just found would reject it again.
    if (!hit->shape->Intersect(r, t * (1.0 + 1e-6) + 1e-6, &hit->t, &hit->si))
        return false;
    if (!hit->si.material) hit->si.material = hit->shape->material;
    return true;
}

bool Occluded(const Scene& scene, const Ray& r, double tMax) {
    for (const auto& s : scene.shapes) {
        double th;
        if (s->IntersectRay(r, tMax, &th)) return true;
    }
    return false;
}

std::unique_ptr<Integrator> MakeIntegrator(const std::string& name) {
    if (name == "distributed" || name == "dr" || name == "drt")
        return std::make_unique<DistributedIntegrator>();
    return std::make_unique<WhittedIntegrator>();
}

}  // namespace cgr
