#include "integrator/whitted.h"

#include <cmath>

#include "core/logger.h"
#include "core/sampler.h"
#include "core/timer.h"
#include "scene/light.h"
#include "scene/material.h"

namespace cgr {

namespace {

struct Refraction {
    Ray ray;
    double etaFactor = 1.0;  // eta_t / eta_i
    bool totalInternalReflection = false;
};

// Snell's law. `n` is oriented to oppose the incident direction.
Refraction Refract(const Ray& r, const Normal& n, double etaI, double etaT) {
    Refraction out;
    double eta = etaT / etaI;
    double cosThetaI = Clamp(Dot(n, r.d), -1.0, 1.0);
    double sin2ThetaI = std::max(0.0, 1.0 - cosThetaI * cosThetaI);
    double sin2ThetaT = sin2ThetaI / (eta * eta);
    if (sin2ThetaT >= 1.0) {
        // Total internal reflection: no transmitted ray exists.
        out.totalInternalReflection = true;
        Vec3 d = Reflect(r.d, n);
        out.ray = Ray(r.o + d * 1e-4, d);
        out.etaFactor = 1.0;
        return out;
    }
    double cosThetaT = std::sqrt(std::max(0.0, 1.0 - sin2ThetaT));
    Vec3 d = Normalize((-r.d + n * cosThetaI) / eta + n * cosThetaT);
    out.ray = Ray(r.o + d * 1e-4, d);
    out.etaFactor = 1.0 / eta;
    return out;
}

Color Background() { return Color(0.02, 0.02, 0.025); }

}  // namespace

Color WhittedIntegrator::Radiance(const Scene& scene, const BVH* bvh, const Ray& ray,
                                  int depth, Color throughput) const {
    // Path termination: at the deepest level only the throughput survives.
    if (depth >= scene.maxDepth) return throughput;

    Hit hit;
    if (!IntersectScene(scene, bvh, ray, kInf, &hit)) return Background() * throughput;

    const Material* mat = hit.si.material;
    if (!mat) return Background() * throughput;

    Vec3 wo = -ray.d;
    Normal n = hit.si.n;
    Normal ng = hit.si.ng;
    // Orient the shading normal towards the viewer, so back faces shade.
    if (Dot(n, wo) < 0) n = -n;
    if (Dot(ng, wo) < 0) ng = -ng;

    // Textured base colour: UV from the primitive, modulated by the map.
    Color baseColor = mat->DiffuseColor();
    if (mat->hasDiffuseTexture)
        baseColor *= mat->TextureAt(hit.si.uv.x, hit.si.uv.y);

    // ---- delta lobes: the only places a Whitted tracer recurses -----------
    if (mat->IsMirror() || mat->IsSpecularDelta()) {
        Color L(0);
        if (mat->IsMirror()) {
            // Ideal mirror: d_r = d - 2 (d . n) n, with the material reflectance
            // acting as the spectral tint.
            Vec3 d = Reflect(ray.d, ng);
            Color t = throughput * mat->reflectance;
            L += Radiance(scene, bvh, Ray(OffsetOrigin(hit.si.p, d), d), depth + 1, t);
        } else {
            // Dielectric. Determine which side the ray arrives from so eta is
            // always >= 1 in the Fresnel call.
            double etaI = 1.0, etaT = mat->Eta();
            bool entering = Dot(ng, ray.d) < 0;
            double R = FrDielectric(Dot(ng, ray.d), entering ? etaT / etaI : etaI / etaT);
            double T = 1.0 - R;

            if (R > 0) {
                Vec3 d = Reflect(ray.d, ng);
                L += Radiance(scene, bvh, Ray(OffsetOrigin(hit.si.p, d), d), depth + 1,
                              throughput * mat->reflectance * R);
            }
            if (T > 0) {
                // etaI/etaT swap depending on the direction of travel.
                double ei = entering ? 1.0 : mat->Eta();
                double et = entering ? mat->Eta() : 1.0;
                Refraction rf = Refract(ray, ng, ei, et);
                // Radiance is scaled by (eta_t/eta_i)^2 across a refraction.
                double s = rf.etaFactor * rf.etaFactor;
                L += Radiance(scene, bvh, rf.ray, depth + 1,
                              throughput * mat->reflectance * T * s);
            }
        }
        if (!mat->emission.IsBlack()) L += mat->emission * throughput;
        return L;
    }

    // ---- direct lighting: ambient + Lambert diffuse + Blinn-Phong --------
    Color L = baseColor * mat->ambientScale;
    for (const auto& lightPtr : scene.lights) {
        const auto* pl = dynamic_cast<const PointLight*>(lightPtr.get());
        if (!pl) continue;  // area lights are a DRT feature

        Vec3 toLight = pl->position - hit.si.p;
        double dist2 = LengthSquared(toLight);
        if (dist2 < 1e-12) continue;
        double dist = std::sqrt(dist2);
        Vec3 wi = toLight / dist;

        Normal ln = n;
        if (Dot(ln, wi) < 0) ln = -ln;  // two-sided surfaces
        double cosTheta = Dot(ln, wi);
        if (cosTheta <= 0) continue;

        // Shadow ray, offset along the shading normal. dist - 2*eps keeps the
        // far end of the segment from re-hitting the light-side geometry.
        double eps = ShadowEps(dist);
        Ray shadowRay(hit.si.p + n * eps, wi);
        if (Occluded(scene, bvh, shadowRay, dist - 2 * eps)) continue;

        // Radiant intensity (W/sr) -> irradiance (W/m^2): I / r^2.
        Color irradiance = pl->intensity / dist2;

        // Lambertian diffuse: f_r * cos / pi.
        L += baseColor * (kInvPi * cosTheta) * irradiance;

        // Blinn-Phong specular using the halfway vector.
        L += mat->BlinnPhongSpecular(ln, wo, wi) * irradiance;
    }

    if (!mat->emission.IsBlack()) L += mat->emission;
    return L * throughput;
}

Image WhittedIntegrator::Render(const Scene& scene, const BVH* bvh) {
    Image film(scene.film.xResolution, scene.film.yResolution, 3);
    const int w = film.xSize, h = film.ySize;
    const int spp = std::max(1, scene.spp);
    Sampler sampler;
    sampler.type = scene.sampler;
    sampler.nSamples = spp;
    sampler.gridN = scene.gridN;
    Logger::Instance().Info("WSRT: " + std::to_string(w) + "x" + std::to_string(h) +
                            ", maxDepth=" + std::to_string(scene.maxDepth) +
                            ", spp=" + std::to_string(spp) +
                            " sampler=" + SamplerTypeName(scene.sampler) +
                            (spp == 1 ? " (single centre sample, no antialiasing)"
                                      : " (supersampled)"));
    {
        // Scoped so the timer is destroyed (and the stage recorded) before the
        // frame time is read back for the log.
        ScopeTimer frame("render");
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                // spp == 1 keeps the single sample at the pixel centre, so the
                // image carries the unfiltered staircase aliasing that the
                // antialiasing task compares against. spp > 1 jitters every
                // sample inside the pixel with the scene's sampler and averages
                // the radiance, which is the supersampling study.
                Color L(0, 0, 0);
                for (int s = 0; s < spp; ++s) {
                    Point2 j = spp == 1 ? Point2(0.5, 0.5) : sampler.Get(s);
                    Ray r = scene.camera.GenerateRay((x + j.x) / w, (h - y - j.y) / h, j);
                    L += Radiance(scene, bvh, r, 0, Color(1));
                }
                L = L / static_cast<double>(spp);
                film.SetPixel(x, y, Clamp(L, 0.0, 1.0));
            }
        }
    }
    Logger::Instance().Info("WSRT frame time: " +
                            std::to_string(Profiler::Instance().Get("render")) + " s");
    return film;
}

}  // namespace cgr