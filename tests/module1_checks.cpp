// Analytic checks: expected values come from geometry/optics, not old renders.
#include <cmath>
#include <iostream>
#include <string>

#include "core/image.h"
#include "core/logger.h"
#include "integrator/distributed.h"
#include "integrator/whitted.h"
#include "scene/mesh.h"
#include "scene/parser.h"

using namespace cgr;

namespace {
int failures = 0;
int checks = 0;
void Check(bool condition, const std::string& label) {
    ++checks;
    std::cout << (condition ? "PASS " : "FAIL ") << label << '\n';
    if (!condition) ++failures;
}
bool Near(double a, double b, double tolerance = 1e-8) {
    return std::isfinite(a) && std::fabs(a - b) <= tolerance;
}
bool Near(Vec3 a, Vec3 b, double tolerance = 1e-8) {
    return Length(a - b) <= tolerance;
}
bool Near(Color a, Color b, double tolerance = 1e-8) {
    return Near(a.r, b.r, tolerance) && Near(a.g, b.g, tolerance) && Near(a.b, b.b, tolerance);
}
}

int main() {
    Logger::Instance().SetEcho(false);
    const Vec3 n(0, 1, 0);
    Check(Near(Reflect(Vec3(0, 1, 0), n), Vec3(0, 1, 0)), "normal-incidence mirror reflects outward");
    Vec3 incident = Normalize(Vec3(1, -1, 0));
    Vec3 reflected = Reflect(-incident, n);
    Check(Near(reflected.x, incident.x) && Near(reflected.y, -incident.y), "mirror preserves tangent, reverses normal component");

    Vec3 transmitted;
    Check(RefractIncident(Vec3(0, -1, 0), n, 1, 1.5, &transmitted) &&
          Near(transmitted, Vec3(0, -1, 0)), "glass normal incidence continues forward");
    incident = Vec3(0.5, -std::sqrt(0.75), 0);
    Check(RefractIncident(incident, n, 1, 1.5, &transmitted) && Near(transmitted.x, 1.0 / 3.0), "Snell: air to glass at 30 degrees");
    Vec3 exited;
    Check(RefractIncident(transmitted, n, 1.5, 1, &exited) && Near(exited, incident), "parallel interfaces restore the incident direction");
    Check(!RefractIncident(Vec3(std::sqrt(0.75), -0.5, 0), n, 1.5, 1, &transmitted), "glass to air at 60 degrees has total internal reflection");
    Check(Near(FrDielectric(1, 1.5), 0.04), "Fresnel normal-incidence air/glass reflectance = 4 percent");
    Check(Near(FrDielectric(0.5, 1.0 / 1.5), 1), "Fresnel TIR reflectance = 1");
    Check(Near(FrDielectric(0, 1), 0), "identical media have no Fresnel reflection, even at grazing incidence");

    Sphere sphere(1);
    sphere.SetTransform(Transform::Scale(2));
    Ray ray(Point3(0, 0, 5), Vec3(0, 0, -1));
    double t;
    SurfaceInteraction si;
    Check(sphere.Intersect(ray, kInf, &t, &si) && Near(t, 3) && Near(si.p, Point3(0, 0, 2)), "scaled sphere keeps world t=3 and world hit z=2");
    Check(!sphere.IntersectRay(ray, 2.9, &t), "scaled sphere behind a shadow segment is not an occluder");
    Transform transform = Transform::RotateY(33) * Transform::Scale(2, 1, 3);
    Vec3 tangent(1, -1, 0), normal = Normalize(Vec3(1, 1, 0));
    Vec3 transformedNormal = transform.TransformNormal(normal);
    Check(Near(Dot(transformedNormal, transform.TransformVector(tangent)), 0) && Near(Length(transformedNormal), 1), "inverse-transpose normal remains perpendicular and unit length");
    Plane plane;
    plane.SetTransform(Transform::Scale(2, 3, 4));
    Check(plane.IntersectRay(ray, kInf, &t) && Near(t, 5), "scaled plane preserves world t");
    Triangle triangle(Point3(0, 0, 0), Point3(1, 0, 0), Point3(0, 1, 0));
    triangle.SetTransform(Transform::Scale(2));
    Check(triangle.Intersect(Ray(Point3(0.5, 0.5, 5), Vec3(0, 0, -1)), kInf, &t, &si) &&
          Near(t, 5) && Near(si.uv.x, 0.25) && Near(si.uv.y, 0.25), "scaled triangle intersection and barycentric UV");
    Point2 uv = sphere.UV(Point3(0, 0, 1), Vec3(0, 0, 1));
    Check(Near(uv.y, 0), "sphere north pole latitude");
    uv = sphere.UV(Point3(0, 1, 0), Vec3(0, 1, 0));
    Check(Near(uv.x, 0.25) && Near(uv.y, 0.5), "sphere equator longitude and latitude");

    Film film;
    film.xResolution = film.yResolution = 100;
    Camera camera;
    camera.ConfigureThinLens(0.2, 10, 60, Point3(0), Point3(0, 0, -1), Vec3(0, 1, 0), film);
    Ray a = camera.GenerateRay(0.9, 0.7, Point2(0.2, 0.3));
    Ray b = camera.GenerateRay(0.9, 0.7, Point2(0.8, 0.9));
    Point3 fa = a((10 - Dot(a.o - camera.position, camera.forward)) / Dot(a.d, camera.forward));
    Point3 fb = b((10 - Dot(b.o - camera.position, camera.forward)) / Dot(b.d, camera.forward));
    Check(Near(fa, fb), "different lens origins converge on the same planar focus point");
    camera.ConfigurePerspective(60, Point3(0), Point3(0, 0, -1), Vec3(1, 0, 0), film);
    Check(Near(camera.CameraUp(), Vec3(1, 0, 0)), "camera honours supplied up vector");
    Transform viewTransform = Transform::LookAt(Point3(3, 2, 5), Point3(0), Vec3(0, 1, 0));
    Check(Near(viewTransform.TransformPoint(Point3(3, 2, 5)), Point3(0)), "LookAt maps camera position to camera-space origin");

    auto material = MakeMaterial(MaterialType::Phong);
    material->reflectance = Color(1);
    material->specular = Color(0.8);
    material->phongExponent = 20;
    // Independently integrate f*cos over a hemisphere using uniform z/azimuth.
    bool conservative = true;
    for (double angle : {0.0, 45.0, 80.0}) {
        Vec3 wo(std::sin(Radians(angle)), 0, std::cos(Radians(angle)));
        Color energy(0);
        const int nz = 256, np = 256;
        for (int iz = 0; iz < nz; ++iz) {
            double z = (iz + 0.5) / nz;
            double radius = std::sqrt(1 - z * z);
            for (int ip = 0; ip < np; ++ip) {
                double phi = 2 * kPi * (ip + 0.5) / np;
                Vec3 wi(radius * std::cos(phi), radius * std::sin(phi), z);
                energy += material->F(Vec3(0, 0, 1), wo, wi) * z * (2 * kPi / (nz * np));
            }
        }
        conservative = conservative && energy.r <= 1.0001 && energy.r >= 0;
    }
    Check(conservative, "Phong hemispherical reflected energy <= incident energy at 0/45/80 degrees");
    material->specular = Color(0);
    Check(Near(material->F(Vec3(0, 0, 1), Vec3(0, 0, 1), Vec3(0, 0, 1), Color(0)), Color(0)), "black diffuse texel yields zero diffuse BRDF");
    Check(Near(ToneMapReinhard(Color(4), 4, true), Color(1)), "extended Reinhard maps white point to display white");
    Check(Near(ToneMapReinhard(Color(0.5), 1, true), Color(0.5)), "Reinhard W=1 identity (not an implementation failure)");

    std::string error;
    auto reportMesh = LoadOBJ("scenes/meshes/uv_geodesic.obj", &error);
    Check(reportMesh && reportMesh->NumTriangles() == 320 &&
          reportMesh->UVs.size() == 960 && reportMesh->N.size() == reportMesh->P.size(),
          "report geodesic mesh has explicit per-face UVs and per-vertex normals");
    if (reportMesh) {
        bool indexed = true;
        for (const auto& face : reportMesh->faces)
            for (int i = 0; i < 3; ++i)
                indexed = indexed && face.vt[i] >= 0 && face.vn[i] == face.v[i];
        Check(indexed, "report mesh corners reference actual UV and normal data");
    }
    auto mesh = LoadOBJ("tests/fixtures/no_normals.obj", &error);
    Check(mesh && mesh->faces[0].vn[1] == mesh->faces[0].v[1], "generated mesh normals receive per-corner indices");
    if (mesh) {
        auto tris = mesh->BuildTriangles(transform);
        Check(tris[0]->hasVertexNormals && Near(tris[0]->sn[0], transform.TransformNormal(mesh->N[0])), "mesh vertex normals transformed with inverse transpose");
    }
    ParseResult parsed = ParseSceneFile("tests/fixtures/module1.pbrt");
    Check(parsed.nErrors == 0 && parsed.nWarnings == 0, "fixture parses without errors/warnings");
    Check(parsed.scene.materials.size() == 4 && Near(parsed.scene.materials[1]->reflectance, Color(0.2, 0.3, 0.4)), "material type does not swallow first reflectance parameter");
    Check(parsed.scene.materials[1]->Type() == MaterialType::BlinnPhong && Near(parsed.scene.materials[2]->phongExponent, 23), "Blinn-Phong type and float phong exponent parsed");
    auto* parsedTriangle = dynamic_cast<Triangle*>(parsed.scene.shapes[1].get());
    Check(parsed.scene.shapes.size() == 3 && parsedTriangle &&
          Near(parsedTriangle->p[0], Point3(4, 0, 0)) &&
          Near(parsedTriangle->p[1], Point3(6, 0, 0)), "OBJ scene translate/scale applied");
    auto* panel = dynamic_cast<AreaLight*>(parsed.scene.lights[0].get());
    Normal ln;
    Point3 corner = panel->SamplePoint(Point2(0, 0), &ln);
    Check(Near(corner, Point3(-1, -1, -2)) && Near(panel->Area(), 4) && panel->samples == 3, "rectangle samples actual corners, area, configurable density");
    Check(Near(panel->Pdf(Point3(0), Vec3(0, 0, -1)), 1), "area-to-solid-angle PDF = distance squared / (area*cos)");
    auto* disc = dynamic_cast<AreaLight*>(parsed.scene.lights[1].get());
    Check(Near(disc->SamplePoint(Point2(0, 0), &ln), Point3(0, 0, -4)), "disc scene transform applied");
    ParseResult broken = ParseSceneFile("tests/fixtures/broken.pbrt");
    Check(broken.nErrors >= 2 && broken.nWarnings >= 1, "missing texture / unknown primitive / unsupported directive produce diagnostics");
    ParseResult standard = ParseSceneFile("tests/fixtures/standard.pbrt");
    Check(standard.nErrors == 0 && standard.nWarnings == 0 && standard.scene.shapes.size() == 3,
          "standard PBRT syntax: statements, scopes and multiline trianglemesh arrays");
    Check(standard.scene.spp == 4 && standard.scene.shapes[0]->material->hasDiffuseTexture,
          "standard PBRT sampler and named spectrum texture");
    Check(standard.scene.shapes[0]->IntersectRay(Ray(Point3(1, 0, 5), Vec3(0, 0, -1)), kInf, &t) && Near(t, 3),
          "standard PBRT Translate/Scale scope affects sphere");
    Check(Near(standard.scene.camera.vMax, std::tan(Radians(20))),
          "standard PBRT landscape FOV applies to smaller (vertical) axis");
    Check(standard.scene.lights.size() == 2 && Near(standard.scene.lights[1]->Area(), 8),
          "standard PBRT point light and four-corner diffuse area emitter");

    Scene mirrorScene;
    mirrorScene.maxDepth = 3;
    auto mirror = MakeMaterial(MaterialType::Mirror);
    mirrorScene.materials.push_back(mirror);
    auto mirrorPlane = std::make_shared<Plane>();
    mirrorPlane->material = mirror.get();
    mirrorScene.shapes.push_back(mirrorPlane);
    WhittedIntegrator wsrt;
    DistributedIntegrator drt;
    Ray view(Point3(0, 0, 1), Vec3(0, 0, -1));
    Check(Near(wsrt.Radiance(mirrorScene, view, 0, Color(1)), Color(0.02, 0.02, 0.025)), "WSRT mirror actually reflects background, not recursion-limit white");
    Check(Near(drt.SampleRadiance(mirrorScene, view, 0, Color(1), 0), Color(0.02, 0.02, 0.025)), "DRT mirror actually reflects background");
    Check(Near(wsrt.Radiance(mirrorScene, view, 3, Color(1)), Color(0)), "WSRT depth termination contributes zero");
    mirrorPlane->material = parsed.scene.materials[3].get();
    mirrorScene.maxDepth = 2;
    double expectedWeight = 0.04 + 0.96 / (1.5 * 1.5);
    Check(Near(wsrt.Radiance(mirrorScene, view, 0, Color(1)), Color(0.02, 0.02, 0.025) * expectedWeight), "WSRT glass includes both Fresnel paths with radiance eta scaling");
    Check(Near(drt.SampleRadiance(mirrorScene, view, 0, Color(1), 0), Color(0.02, 0.02, 0.025) * expectedWeight), "DRT glass includes both Fresnel paths");

    Texture2D texture;
    texture.width = texture.height = 1;
    texture.data = {0.2, 0.4, 0.6};
    texture.wrapS = texture.wrapT = TextureWrap::Black;
    Check(Near(TextureLookup(texture, 0.5, 0.5), Color(0.2, 0.4, 0.6)) &&
          Near(TextureLookup(texture, 2, 2), Color(0)), "black texture wrap preserves in-bounds texels, blacks only outside");

    // A flat diffuse surface illuminated head-on by a point light has an
    // exact analytic answer, independent of any image baseline.
    Scene litScene;
    auto diffuse = MakeMaterial(MaterialType::Phong);
    diffuse->reflectance = Color(0.5);
    diffuse->specular = Color(0);
    diffuse->ambientScale = Color(0);
    litScene.materials.push_back(diffuse);
    auto floor = std::make_shared<Plane>();
    floor->material = diffuse.get();
    litScene.shapes.push_back(floor);
    auto point = std::make_shared<PointLight>(Point3(0, 0, 2));
    point->intensity = Color(4);
    litScene.lights.push_back(point);
    Check(Near(wsrt.Radiance(litScene, view, 0, Color(1)), Color(0.5 * kInvPi)) &&
          Near(drt.SampleRadiance(litScene, view, 0, Color(1), 0), Color(0.5 * kInvPi)), "WSRT/DRT diffuse light equals rho/pi * I/r^2, no self-shadow");
    auto blocker = std::make_shared<Sphere>(0.2);
    blocker->SetTransform(Transform::Translate(Point3(0, 0, 1.5)));
    blocker->material = diffuse.get();
    litScene.shapes.push_back(blocker);
    Check(Near(wsrt.Radiance(litScene, view, 0, Color(1)), Color(0)) &&
          Near(drt.SampleRadiance(litScene, view, 0, Color(1), 0), Color(0)), "occluder between surface and light blocks direct illumination");

    // Glass EXIT test: an inside ray must select glass->air, not air->glass.
    Scene glassScene;
    glassScene.maxDepth = 1;
    auto glass = MakeMaterial(MaterialType::Dielectric);
    glassScene.materials.push_back(glass);
    auto glassSphere = std::make_shared<Sphere>(1);
    glassSphere->material = glass.get();
    glassScene.shapes.push_back(glassSphere);
    Ray inside(Point3(0), Vec3(0, 0, 1));
    glassScene.maxDepth = 2;
    Color exitExpected = Color(0.02, 0.02, 0.025) * (0.96 * 1.5 * 1.5);
    Check(Near(wsrt.Radiance(glassScene, inside, 0, Color(1)), exitExpected) &&
          Near(drt.SampleRadiance(glassScene, inside, 0, Color(1), 0), exitExpected), "inside glass ray selects exit medium and reciprocal eta scaling");

    Scene emitterScene;
    auto emitter = std::make_shared<AreaLight>();
    emitter->emission = Color(4, 5, 6);
    emitterScene.lights.push_back(emitter);
    Check(Near(drt.SampleRadiance(emitterScene, view, 0, Color(1), 0), Color(4, 5, 6)), "area emitter is visible to camera and recursive rays");

    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
