#include "integrator/distributed.h"

#include <cmath>

#include "core/logger.h"
#include "core/timer.h"
#include "scene/light.h"
#include "scene/material.h"

namespace cgr {

namespace {

Color Background() { return Color(0.02, 0.02, 0.025); }

}  // namespace

Point2 DistributedIntegrator::NextSample(const Scene& scene, uint64_t index) const {
    return Sample2D(index, scene.spp);
}

Color DistributedIntegrator::SampleRadiance(const Scene& scene,
                                            const Ray& ray, int depth, Color beta,
                                            uint64_t sampleIndex) const {
    if (depth >= scene.maxDepth) return Color(0);

    Hit hit;
    bool foundSurface = IntersectScene(scene, ray, kInf, &hit);
    double closest = foundSurface ? hit.t : kInf;
    const AreaLight* emitter = nullptr;
    Normal emitterNormal;
    for (const auto& light : scene.lights) {
        const auto* al = dynamic_cast<const AreaLight*>(light.get());
        double t;
        Normal ln;
        if (al && al->IntersectRay(ray, closest, &t, &ln)) {
            closest = t;
            emitter = al;
            emitterNormal = ln;
        }
    }
    if (emitter) {
        // This subset models two-sided emitters, consistently with direct sampling.
        if (Dot(emitterNormal, -ray.d) < 0) emitterNormal = -emitterNormal;
        return emitter->L(ray(closest), emitterNormal, -ray.d) * beta;
    }
    if (!foundSurface) return Background() * beta;

    const Material* mat = hit.si.material;
    if (!mat) return Color(0);
    if (hit.shape->IsEmissive()) return hit.shape->emission * beta;

    Vec3 wo = -ray.d;
    Normal n = hit.si.n;
    if (Dot(n, wo) < 0) n = -n;

    // Base albedo (textured where a map is present).
    Color baseColor = mat->DiffuseColor();
    if (mat->hasDiffuseTexture)
        baseColor *= mat->TextureAt(hit.si.uv.x, hit.si.uv.y);

    // ---- delta lobes -----------------------------------------------------
    if (mat->IsMirror() || mat->IsSpecularDelta()) {
        Color L(0);
        if (mat->IsMirror()) {
            Vec3 d = Reflect(wo, hit.si.ng);
            L += SampleRadiance(scene, Ray(OffsetOrigin(hit.si.p, d), d), depth + 1,
                                beta * mat->reflectance, sampleIndex);
        } else {
            bool entering = Dot(hit.si.ng, ray.d) < 0;
            double ei = entering ? 1.0 : mat->Eta();
            double et = entering ? mat->Eta() : 1.0;
            Normal faceNormal = entering ? hit.si.ng : -hit.si.ng;
            double R = FrDielectric(Dot(faceNormal, wo), et / ei);
            double T = 1.0 - R;
            if (R > 0) {
                Vec3 d = Reflect(wo, faceNormal);
                L += SampleRadiance(scene, Ray(OffsetOrigin(hit.si.p, d), d), depth + 1,
                                    beta * mat->reflectance * R, sampleIndex);
            }
            Vec3 d;
            if (T > 0 && RefractIncident(ray.d, faceNormal, ei, et, &d)) {
                double eta = ei / et;
                L += SampleRadiance(scene, Ray(OffsetOrigin(hit.si.p, d), d), depth + 1,
                                    beta * mat->reflectance * T * eta * eta, sampleIndex);
            }
        }
        if (!mat->emission.IsBlack()) L += mat->emission * beta;
        return L;
    }

    // ---- direct lighting from area lights (soft shadows) -----------------
    Color L(0);
    for (size_t lightIndex = 0; lightIndex < scene.lights.size(); ++lightIndex) {
        const auto& lightPtr = scene.lights[lightIndex];
        const auto* al = dynamic_cast<const AreaLight*>(lightPtr.get());
        if (!al) continue;
        for (int ls = 0; ls < al->samples; ++ls) {
            Normal ln;
            uint64_t index = sampleIndex * 6364136223846793005ULL +
                             static_cast<uint64_t>(depth) * 7919ULL +
                             lightIndex * 104729ULL + static_cast<uint64_t>(ls);
            Point3 lp = al->SamplePoint(NextSample(scene, index), &ln);
            // Orient the light normal towards the shading point.
            if (Dot(ln, hit.si.p - lp) < 0) ln = -ln;
            Vec3 d = lp - hit.si.p;
            double dist2 = LengthSquared(d);
            if (dist2 < 1e-12) continue;
            double dist = std::sqrt(dist2);
            Vec3 wi = d / dist;

            Normal ln2 = n;
            double cosSurface = Dot(ln2, wi);
            if (cosSurface <= 0) continue;
            double cosLight = Dot(ln, -wi);
            if (cosLight <= 0) continue;

            // Shadow ray (offset to avoid self-shadowing).
            double eps = ShadowEps(dist);
            Normal offsetNormal = Dot(hit.si.ng, wi) >= 0 ? hit.si.ng : -hit.si.ng;
            Ray shadow(hit.si.p + offsetNormal * eps, wi);
            if (Occluded(scene, shadow, dist - 2 * eps)) continue;

            // Convert the area density to a solid-angle one.
            double area = al->Area();
            if (area <= 0) continue;
            double pdfLight = dist2 / (cosLight * area);

            // Le (radiance leaving the light). L() wants the direction the
            // radiance travels, which is -wi here since wi points at the light.
            Color Le = al->L(lp, ln, -wi);
            if (Le.IsBlack()) continue;

            // f * cos * Le / pdf, for a cosine-weighted diffuse lobe plus the
            // Phong specular lobe.
            Color f = mat->F(n, wo, wi, baseColor);
            L += f * cosSurface * Le / pdfLight * beta / static_cast<double>(al->samples);
        }
    }
    {
        // Point and area lights can coexist in the same scene.
        for (const auto& lightPtr : scene.lights) {
            const auto* pl = dynamic_cast<const PointLight*>(lightPtr.get());
            if (!pl) continue;
            Vec3 d = pl->position - hit.si.p;
            double dist2 = LengthSquared(d);
            if (dist2 < 1e-12) continue;
            double dist = std::sqrt(dist2);
            Vec3 wi = d / dist;
            Normal ln2 = n;
            double cosSurface = Dot(ln2, wi);
            if (cosSurface <= 0) continue;
            double eps = ShadowEps(dist);
            Normal offsetNormal = Dot(hit.si.ng, wi) >= 0 ? hit.si.ng : -hit.si.ng;
            if (Occluded(scene, Ray(hit.si.p + offsetNormal * eps, wi), dist - 2 * eps))
                continue;
            Color irradiance = pl->intensity / dist2;
            Color f = mat->F(n, wo, wi, baseColor);
            L += f * cosSurface * irradiance * beta;
        }
    }

    // Module 1 evaluates direct lighting plus delta reflection/refraction.
    // Diffuse indirect illumination and its BF study belong to later modules.
    L += mat->emission * beta;

    return L;
}

Image DistributedIntegrator::Render(const Scene& scene) {
    Image film(scene.film.xResolution, scene.film.yResolution, 3);
    const int w = film.xSize, h = film.ySize;
    const int spp = std::max(1, scene.spp);
    Logger::Instance().Info("DRT: " + std::to_string(w) + "x" + std::to_string(h) +
                            ", maxDepth=" + std::to_string(scene.maxDepth) + " BF=" +
                            std::to_string(scene.branchingFactor) + " spp=" +
                            std::to_string(spp) + " sampler=" +
                            SamplerTypeName(scene.sampler) + " lensRadius=" +
                            std::to_string(scene.camera.lensRadius) + " focus=" +
                            std::to_string(scene.camera.focusDistance));
    {
        // Scoped so the timer is destroyed (and the stage recorded) before the
        // frame time is read back for the log.
        ScopeTimer frame("render");
        Color sum(0, 0, 0);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                sum = Color(0, 0, 0);
                for (int s = 0; s < spp; ++s) {
                    uint64_t index = (static_cast<uint64_t>(y) * w + x) * spp + s;
                    Point2 lens = NextSample(scene, index ^ 0xa0761d6478bd642fULL);
                    // Pixel centres keep the Module 1 baseline unfiltered;
                    // lens and light positions are independently sampled.
                    // Film rows run top-down while the screen window runs up.
                    double sy = (h - y - 0.5) / h;
                    Ray r = scene.camera.GenerateRay((x + 0.5) / w, sy, lens);
                    sum += SampleRadiance(scene, r, 0, Color(1), index);
                }
                Color L = sum / static_cast<double>(spp);
                // Reinhard tone mapping (per channel) applied here on the HDR
                // value, then clamped, so the written image is display-ready.
                Color tm = ToneMapReinhard(L, scene.whitePoint, scene.perChannelToneMap);
                film.SetPixel(x, y, Clamp(tm, 0.0, 1.0));
            }
        }
    }
    Logger::Instance().Info("DRT frame time: " +
                            std::to_string(Profiler::Instance().Get("render")) + " s");
    return film;
}

}  // namespace cgr
