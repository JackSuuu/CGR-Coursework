#include "integrator/whitted.h"

#include <cmath>

#include "core/logger.h"
#include "core/timer.h"
#include "scene/light.h"
#include "scene/material.h"

namespace cgr {

namespace {

Color Background() { return Color(0.02, 0.02, 0.025); }

}  // namespace

Color WhittedIntegrator::Radiance(const Scene& scene, const Ray& ray,
                                  int depth, Color throughput) const {
    // A terminated path contributes no invented light.
    if (depth >= scene.maxDepth) return Color(0);

    Hit hit;
    if (!IntersectScene(scene, ray, kInf, &hit)) return Background() * throughput;

    const Material* mat = hit.si.material;
    if (!mat) return Background() * throughput;

    Vec3 wo = -ray.d;
    Normal n = hit.si.n;
    Normal ng = hit.si.ng; // keep outward orientation for dielectric entry/exit
    // Orient the shading normal towards the viewer, so back faces shade.
    if (Dot(n, wo) < 0) n = -n;

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
            Vec3 d = Reflect(wo, ng);
            Color t = throughput * mat->reflectance;
            L += Radiance(scene, Ray(OffsetOrigin(hit.si.p, d), d), depth + 1, t);
        } else {
            // Determine the medium BEFORE orienting the geometric normal.
            bool entering = Dot(ng, ray.d) < 0;
            double etaI = entering ? 1.0 : mat->Eta();
            double etaT = entering ? mat->Eta() : 1.0;
            Normal faceNormal = entering ? ng : -ng;
            double R = FrDielectric(Dot(faceNormal, wo), etaT / etaI);
            double T = 1.0 - R;

            if (R > 0) {
                Vec3 d = Reflect(wo, faceNormal);
                L += Radiance(scene, Ray(OffsetOrigin(hit.si.p, d), d), depth + 1,
                              throughput * mat->reflectance * R);
            }
            if (T > 0) {
                Vec3 d;
                if (RefractIncident(ray.d, faceNormal, etaI, etaT, &d)) {
                    double eta = etaI / etaT;
                    L += Radiance(scene, Ray(OffsetOrigin(hit.si.p, d), d), depth + 1,
                                  throughput * mat->reflectance * T * eta * eta);
                }
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
        double cosTheta = Dot(ln, wi);
        if (cosTheta <= 0) continue;

        // Shadow ray, offset along the shading normal. dist - 2*eps keeps the
        // far end of the segment from re-hitting the light-side geometry.
        double eps = ShadowEps(dist);
        Normal offsetNormal = Dot(ng, wi) >= 0 ? ng : -ng;
        Ray shadowRay(hit.si.p + offsetNormal * eps, wi);
        if (Occluded(scene, shadowRay, dist - 2 * eps)) continue;

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

Image WhittedIntegrator::Render(const Scene& scene) {
    Image film(scene.film.xResolution, scene.film.yResolution, 3);
    const int w = film.xSize, h = film.ySize;
    Logger::Instance().Info("WSRT: " + std::to_string(w) + "x" + std::to_string(h) +
                            ", maxDepth=" + std::to_string(scene.maxDepth) +
                            ", 1 sample/pixel (no antialiasing)");
    {
        // Scoped so the timer is destroyed (and the stage recorded) before the
        // frame time is read back for the log.
        ScopeTimer frame("render");
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                // Single sample at the pixel centre. The WSRT is left unfiltered
                // on purpose: the aliasing that produces is the subject of
                // Module 2.
                Ray r = scene.camera.GenerateRay((x + 0.5) / w, (h - y - 0.5) / h,
                                                Point2(0.5, 0.5));
                Color L = Radiance(scene, r, 0, Color(1));
                film.SetPixel(x, y, Clamp(L, 0.0, 1.0));
            }
        }
    }
    Logger::Instance().Info("WSRT frame time: " +
                            std::to_string(Profiler::Instance().Get("render")) + " s");
    return film;
}

}  // namespace cgr
