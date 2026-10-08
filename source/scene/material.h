#pragma once
// Materials for both integrators.
//
//   * Diffuse / Lambert       -- WSRT "diffuse" component
//   * BlinnPhong              -- WSRT requirement (ambient + diffuse + specular)
//   * Phong                   -- DRT requirement, energy-conserving variant
//   * Mirror                  -- WSRT ideal specular reflection
//   * Dielectric              -- WSRT Snell + Fresnel + total internal reflection
//
// Each material exposes both an analytic evaluation (used by the WSRT) and the
// components the DRT needs (pdf + sample + f), so the two integrators can share
// the same objects.
#include <memory>
#include <string>

#include "core/color.h"
#include "core/texture.h"
#include "core/vec.h"

namespace cgr {

struct Ray;

enum class MaterialType { Diffuse, BlinnPhong, Phong, Mirror, Dielectric, Plastic };

class Material {
  public:
    virtual ~Material() = default;

    // Diffuse colour (modulated by the diffuse texture when present).
    virtual Color DiffuseColor() const { return reflectance; }
    virtual Color SpecularColor() const { return Color(0); }
    // Is the surface an ideal mirror?
    virtual bool IsMirror() const { return false; }
    // Is the surface perfectly smooth (delta) rather than glossy?
    virtual bool IsSpecularDelta() const { return false; }
    virtual double Eta() const { return 1.0; }

    // ---- shared lobes ---------------------------------------------------
    // The two classic specular models, evaluated for a unit-radiance
    // contribution. Shared by both integrators so the models are implemented
    // exactly once.
    virtual Color BlinnPhongSpecular(const Normal& n, Vec3 wo, Vec3 wi) const;
    virtual Color PhongSpecular(const Normal& n, Vec3 wo, Vec3 wi) const;

    // ---- DRT interface --------------------------------------------------
    // Which lobe family the glossy term uses; concrete materials override it.
    virtual MaterialType Type() const { return MaterialType::Diffuse; }
    // f(wo, wi): Lambertian term plus an energy-conserving glossy lobe. Both
    // wo and wi point away from the surface, into the visible hemisphere of n.
    virtual Color F(const Normal& n, Vec3 wo, Vec3 wi) const;
    // Same BRDF with the texture-modulated diffuse colour at this intersection.
    Color F(const Normal& n, Vec3 wo, Vec3 wi, Color diffuse) const;

    virtual std::string Name() const = 0;

    // ---- shared parameters ---------------------------------------------
    Color reflectance = Color(0.8);
    Color specular = Color(0.0);
    Color emission = Color(0);
    Color ambientScale = Color(0.05);
    double shininess = 100.0;  // Blinn-Phong exponent
    double phongExponent = 40.0;
    double eta = 1.5;
    bool hasDiffuseTexture = false;
    TexturePtr diffuseTexture;
    Color textureModulate = Color(1);

    void SetDiffuseTexture(const TexturePtr& t) {
        diffuseTexture = t;
        hasDiffuseTexture = (t != nullptr);
    }
    // Texture lookup honouring the stored scale / wrap settings.
    Color TextureAt(double u, double v) const {
        if (!hasDiffuseTexture || !diffuseTexture->IsValid()) return Color(1);
        return TextureLookup(*diffuseTexture, u, v) * textureModulate;
    }
    Color TextureAtNearest(double u, double v) const {
        if (!hasDiffuseTexture || !diffuseTexture->IsValid()) return Color(1);
        return TextureLookupNearest(*diffuseTexture, u, v) * textureModulate;
    }
};

using MaterialPtr = std::shared_ptr<Material>;

std::shared_ptr<Material> MakeMaterial(MaterialType t);

// Unpolarised Fresnel reflectance. cosThetaI >= 0, eta = etaT / etaI.
double FrDielectric(double cosThetaI, double eta);

}  // namespace cgr
