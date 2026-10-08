#pragma once
// Camera with three projection models, all driven by the same rasterisation
// loop so the primary-ray generation differs in one place only:
//
//   * Perspective   -- pinhole, screen window from hfov
//   * Orthographic  -- parallel rays, screen window from the ortho width
//   * Thin lens     -- perspective rays jittered over a disk aperture
#include <memory>

#include "core/sampler.h"
#include "core/transform.h"
#include "core/vec.h"

namespace cgr {

enum class ProjectionType { Perspective, Orthographic };

struct Film {
    int xResolution = 1280;
    int yResolution = 720;
    double pixelAspect = 1.0;
    // Set to (xResolution, yResolution) when the window is derived from the
    // resolution and fov; otherwise a fixed world-space window is used.
    bool hasExplicitWindow = false;
    double xMin = 0, xMax = 1, yMin = 0, yMax = 1;
};

class Camera {
  public:
    Camera() = default;

    void ConfigurePerspective(double fovDegrees, const Point3& pos, Point3 look, Vec3 up,
                              const Film& film);
    void ConfigureOrthographic(double orthoWidth, const Point3& pos, Point3 look, Vec3 up,
                               const Film& film);
    // Turn the camera into a thin-lens camera. `focusDistance` is measured along
    // the viewing direction (0 = derive from the look-at target). The field of
    // view is passed in explicitly so that enabling the lens does not silently
    // replace the value declared in the scene file.
    void ConfigureThinLens(double apertureRadius, double focusDistance, double fovDegrees,
                           const Point3& pos, Point3 look, Vec3 up, const Film& film);

    // Generate the primary ray for the continuous film position (sx, sy) in
    // [0,1]x[0,1] with a lens sample in the unit square, mapped onto the disk.
    Ray GenerateRay(double sx, double sy, Point2 lensSample) const;

    // Camera basis in world space.
    Vec3 CameraRight() const;
    Vec3 CameraUp() const;

    Transform CameraToWorld;
    Point3 position = Point3(0, 0, 1);
    Vec3 forward = Vec3(0, 0, -1);
    ProjectionType type = ProjectionType::Perspective;

    double fov = 90.0;          // degrees, applied to the larger image axis
    double uMin = 0, uMax = 1;  // screen window at distance 1 (perspective)
    double vMin = 0, vMax = 1;
    double orthoScale = 1.0;    // world units across the larger axis
    double lensRadius = 0.0;    // 0 => pinhole
    double focusDistance = 1.0;

    // ---- remembered setup ------------------------------------------------
    // The configure calls record their arguments so that a single parameter
    // can be changed later (the command line overrides for aperture, focal
    // distance and field of view) without repeating the look-at.
    enum class Setup { None, Perspective, Orthographic, ThinLens };
    Setup lastSetup = Setup::None;
    double setupFov = 90.0;
    double setupOrthoWidth = 1.0;
    double setupAperture = 0.0;
    double setupFocus = 0.0;
    Point3 setupPos, setupLook;
    Vec3 setupUp = Vec3(0, 1, 0);
    Film setupFilm;
    // Re-applies the remembered setup, picking up any edited parameter.
    void Reconfigure();

    std::string ProjectionName() const {
        if (lensRadius > 0) return "thin-lens perspective";
        return type == ProjectionType::Perspective ? "perspective" : "orthographic";
    }
};

}  // namespace cgr
