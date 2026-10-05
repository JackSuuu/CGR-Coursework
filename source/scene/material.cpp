#include "scene/material.h"

#include <algorithm>
#include <cmath>

namespace cgr {

double FrDielectric(double cosThetaI, double eta) {
    // eta is the relative IOR (etaI / etaT). cosThetaI is measured against the
    // normal on the side the incident ray is on.
    cosThetaI = Clamp(cosThetaI, -1.0, 1.0);
    if (cosThetaI < 0) {
        eta = 1.0 / eta;
        cosThetaI = -cosThetaI;
    }
    double sin2ThetaI = 1.0 - cosThetaI * cosThetaI;
    double sin2ThetaT = sin2ThetaI / (eta * eta);
    if (sin2ThetaT >= 1.0) return 1.0;  // total internal reflection
    double cosThetaT = std::sqrt(std::max(0.0, 1.0 - sin2ThetaT));
    double rParl = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);
    double rPerp = (cosThetaI - eta * cosThetaT) / (cosThetaI + eta * cosThetaT);
    return 0.5 * (rParl * rParl + rPerp * rPerp);
}

//
// The two classic specular lobes. BlinnPhongSpecular uses the halfway vector
// (cos h)^shininess; PhongSpecular reflects wi about n and compares with wo,
// which is the model the DRT is required to use. Both return the specular
// factor for unit incoming radiance, so the integrators multiply by whatever
// light quantity they already hold.
//
Color Material::BlinnPhongSpecular(const Normal& n, Vec3 wo, Vec3 wi) const {
    if (specular.IsBlack()) return Color(0);
    Vec3 h = Normalize(wo + wi);
    double cosThetaH = AbsDot(n, h);
    if (cosThetaH <= 0) return Color(0);
    return specular * std::pow(cosThetaH, shininess);
}

Color Material::PhongSpecular(const Normal& n, Vec3 wo, Vec3 wi) const {
    if (specular.IsBlack()) return Color(0);
    double cosThetaR = Dot(Reflect(wi, n), wo);
    if (cosThetaR <= 0) return Color(0);
    return specular * std::pow(cosThetaR, phongExponent);
}

Color Material::F(const Normal& n, Vec3 wo, Vec3 wi) const {
    // Both directions are expected in the hemisphere around n (the integrators
    // flip n to face wo before calling).
    double cosTheta = Dot(n, wi);
    if (cosTheta <= 0) return Color(0);
    // Lambertian base: reflectance / pi.
    Color f = reflectance * kInvPi;
    if (!specular.IsBlack()) {
        bool phong = (Type() == MaterialType::Phong);
        double e = phong ? phongExponent : shininess;
        // The lobe helpers return the raw cos^e term, so the normalising
        // (e + 2) / (2 pi) factor is applied here to keep the lobe energy
        // independent of the exponent.
        Color lobe = phong ? PhongSpecular(n, wo, wi) : BlinnPhongSpecular(n, wo, wi);
        f += lobe * ((e + 2.0) / (2.0 * kPi));
    }
    return f;
}

//
// Concrete materials. A single class with a type tag keeps the parser simple;
// the behaviour differences all live in the virtuals above.
//
class StandardMaterial : public Material {
  public:
    explicit StandardMaterial(MaterialType t) : type(t) {
        switch (t) {
            case MaterialType::Diffuse:
                reflectance = Color(0.8);
                ambientScale = Color(0.05);
                break;
            case MaterialType::BlinnPhong:
                reflectance = Color(0.6);
                specular = Color(0.3);
                shininess = 100.0;
                ambientScale = Color(0.05);
                break;
            case MaterialType::Phong:
                reflectance = Color(0.6);
                specular = Color(0.2);
                phongExponent = 40.0;
                ambientScale = Color(0.05);
                break;
            case MaterialType::Mirror:
                reflectance = Color(1, 1, 1);
                break;
            case MaterialType::Dielectric:
                reflectance = Color(1, 1, 1);
                eta = 1.5;
                break;
            case MaterialType::Plastic:
                reflectance = Color(0.6);
                specular = Color(0.2);
                shininess = 100.0;
                break;
        }
    }

    std::string Name() const override {
        switch (type) {
            case MaterialType::Diffuse:   return "diffuse";
            case MaterialType::BlinnPhong: return "blinnphong";
            case MaterialType::Phong:     return "phong";
            case MaterialType::Mirror:    return "mirror";
            case MaterialType::Dielectric: return "dielectric";
            case MaterialType::Plastic:   return "plastic";
        }
        return "unknown";
    }

    MaterialType Type() const override { return type; }
    bool IsMirror() const override { return type == MaterialType::Mirror; }
    bool IsSpecularDelta() const override { return type == MaterialType::Dielectric; }
    double Eta() const override { return eta; }

    Color DiffuseColor() const override { return reflectance; }
    Color SpecularColor() const override { return specular; }

  private:
    MaterialType type;
};

std::shared_ptr<Material> MakeMaterial(MaterialType t) {
    return std::make_shared<StandardMaterial>(t);
}

}  // namespace cgr
