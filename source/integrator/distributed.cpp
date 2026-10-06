#include "integrator/distributed.h"

#include <cmath>

#include "core/logger.h"
#include "core/timer.h"
#include "scene/light.h"
#include "scene/material.h"

namespace cgr {

namespace {

// Cosine-weighted hemisphere sample around +z.
Point3 CosineHemisphere(double u1, double u2) {
    // Concentric mapping: r = sqrt(u1), phi = 2 pi u2, z = sqrt(1-u1).
    double r = std::sqrt(u1);
    double phi = 2.0 * kPi * u2;
    return {r * std::cos(phi), r * std::sin(phi), std::sqrt(std::max(0.0, 1 - u1))};
}

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
    if (!IntersectScene(scene, ray, kInf, &hit)) return Background() * beta;

    const Material* mat = hit.si.material;
    if (!mat) return Color(0);
    if (hit.shape->IsEmissive() && !mat->emission.IsBlack())
        return mat->emission * beta;

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
        Ray next;
        double weight = 1.0;
        if (mat->IsMirror()) {
            Vec3 d = Reflect(ray.d, hit.si.ng);
            next = Ray(OffsetOrigin(hit.si.p, d), d);
            weight = 1.0;
        } else {
            bool entering = Dot(hit.si.ng, ray.d) < 0;
            double ei = entering ? 1.0 : mat->Eta();
            double et = entering ? mat->Eta() : 1.0;
            double R = FrDielectric(Dot(hit.si.ng, ray.d),
                                    entering ? et / ei : ei / et);
            double T = 1.0 - R;
            if (R >= T) {
                Vec3 d = Reflect(ray.d, hit.si.ng);
                next = Ray(OffsetOrigin(hit.si.p, d), d);
                weight = R;
            } else {
                double eta = et / ei;
                double cosThetaI = Clamp(Dot(hit.si.ng, ray.d), -1.0, 1.0);
                double sin2ThetaI = std::max(0.0, 1.0 - cosThetaI * cosThetaI);
                double sin2ThetaT = sin2ThetaI / (eta * eta);
                if (sin2ThetaT >= 1.0) {
                    Vec3 d = Reflect(ray.d, hit.si.ng);
                    next = Ray(OffsetOrigin(hit.si.p, d), d);
                    weight = 1.0;
                } else {
                    double cosThetaT = std::sqrt(std::max(0.0, 1.0 - sin2ThetaT));
                    Vec3 d = Normalize((-ray.d + hit.si.ng * cosThetaI) / eta +
                                       hit.si.ng * cosThetaT);
                    next = Ray(OffsetOrigin(hit.si.p, d), d);
                    weight = T / (eta * eta);
                }
            }
        }
        if (weight > 0)
            L += SampleRadiance(scene, next, depth + 1, beta * mat->reflectance * weight,
                                sampleIndex + depth * 7919ULL);
        if (!mat->emission.IsBlack()) L += mat->emission * beta;
        return L;
    }

    // ---- direct lighting from area lights (soft shadows) -----------------
    Color L(0);
    // Local frame around the shading normal, reused by the bounce sampling.
    Frame frame(n);

    double nLights = 0;
    for (const auto& l : scene.lights)
        if (l->IsArea()) ++nLights;
    if (nLights > 0) {
        for (const auto& lightPtr : scene.lights) {
            const auto* al = dynamic_cast<const AreaLight*>(lightPtr.get());
            if (!al) continue;
            Normal ln;
            Point3 lp = al->SamplePoint(NextSample(scene, sampleIndex + depth), &ln);
            // Orient the light normal towards the shading point.
            if (Dot(ln, hit.si.p - lp) < 0) ln = -ln;
            Vec3 d = lp - hit.si.p;
            double dist2 = LengthSquared(d);
            if (dist2 < 1e-12) continue;
            double dist = std::sqrt(dist2);
            Vec3 wi = d / dist;

            Normal ln2 = n;
            if (Dot(ln2, wi) < 0) ln2 = -ln2;
            double cosSurface = Dot(ln2, wi);
            if (cosSurface <= 0) continue;
            double cosLight = Dot(ln, -wi);
            if (cosLight <= 0) continue;

            // Shadow ray (offset to avoid self-shadowing).
            double eps = ShadowEps(dist);
            Ray shadow(hit.si.p + n * eps, wi);
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
            Color f = mat->F(n, wo, wi);
            L += f * cosSurface * Le / pdfLight * beta;
        }
    } else {
        // No area lights: fall back to point lights so a DRT scene still lights.
        for (const auto& lightPtr : scene.lights) {
            const auto* pl = dynamic_cast<const PointLight*>(lightPtr.get());
            if (!pl) continue;
            Vec3 d = pl->position - hit.si.p;
            double dist2 = LengthSquared(d);
            double dist = std::sqrt(dist2);
            Vec3 wi = d / dist;
            Normal ln2 = n;
            if (Dot(ln2, wi) < 0) ln2 = -ln2;
            double cosSurface = Dot(ln2, wi);
            if (cosSurface <= 0) continue;
            double eps = ShadowEps(dist);
            if (Occluded(scene, Ray(hit.si.p + n * eps, wi), dist - 2 * eps))
                continue;
            Color irradiance = pl->intensity / dist2;
            Color f = mat->F(n, wo, wi);
            L += f * cosSurface * irradiance * beta;
        }
    }

    L += baseColor * mat->ambientScale * beta;

    // ---- indirect bounce: cosine-weighted BRDF sampling -------------------
    if (depth + 1 < scene.maxDepth) {
        // BF rays, each with its own cosine-weighted direction.
        for (int b = 0; b < scene.branchingFactor; ++b) {
            uint64_t s = sampleIndex + static_cast<uint64_t>(b) * 104729ULL +
                         static_cast<uint64_t>(depth) * 7919ULL;
            Point2 uv = NextSample(scene, s);
            Vec3 wiLocal = CosineHemisphere(uv.x, uv.y);
            // pdf for cosine-weighted sampling is cos/pi.
            double pdf = wiLocal.z * kInvPi;
            if (pdf <= 0) continue;
            Vec3 wi = frame.FromLocal(wiLocal);
            // Only continue if the sampled direction is on the visible side.
            if (Dot(n, wi) <= 0) continue;

            Color f = mat->F(n, wo, wi);
            if (f.IsBlack()) continue;

            // Shadow ray for the next vertex.
            double eps = ShadowEps(Length(wi));
            if (Occluded(scene, Ray(hit.si.p + n * eps, wi), kInf)) continue;

            // For a cosine-weighted lobe f*cos/pdf collapses to the albedo, so
            // the throughput multiplier is simply baseColor. The Phong specular
            // lobe is only used for direct lighting here; sampling it is the
            // subject of the final-phase importance-sampling work.
            Ray next(OffsetOrigin(hit.si.p, wi), wi);
            L += SampleRadiance(scene, next, depth + 1, beta * baseColor, s);
        }
    }

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
                    // Stratify over the pixel; the same sample also drives the
                    // lens and the light surface.
                    Point2 lens = NextSample(scene, s);
                    // Film rows run top-down while the screen window runs up.
                    double sy = (h - y - lens.y) / h;
                    Ray r = scene.camera.GenerateRay((x + lens.x) / w, sy, lens);
                    sum += SampleRadiance(scene, r, 0, Color(1), s);
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
