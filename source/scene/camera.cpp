#include "scene/camera.h"

#include <cmath>

namespace cgr {

namespace {

// Build the screen window from the film and the field of view. pbrt applies the
// fov to the larger of the two resolution axes, which is why fov is described
// as "horizontal or vertical" depending on the aspect ratio.
void WindowFromFov(const Film& film, double fovDegrees, bool perspective,
                   double* uMin, double* uMax, double* vMin, double* vMax,
                   double* orthoScale) {
    if (film.hasExplicitWindow) {
        *uMin = film.xMin;
        *uMax = film.xMax;
        *vMin = film.yMin;
        *vMax = film.yMax;
        double s = std::max(*uMax - *uMin, *vMax - *vMin);
        *orthoScale = s;
        return;
    }
    double aspect = (film.pixelAspect * film.xResolution) / film.yResolution;
    double half = std::tan(Radians(fovDegrees) * 0.5);
    if (perspective) {
        // Screen at distance 1.
        if (aspect >= 1.0) {
            *uMin = -half;
            *uMax = half;
            *vMin = -half / aspect;
            *vMax = half / aspect;
        } else {
            *uMin = -half * aspect;
            *uMax = half * aspect;
            *vMin = -half;
            *vMax = half;
        }
    } else {
        // Orthographic: a world-space window, the fov interpreted as a scale on
        // the larger axis (2*half == 1 at fov = 53.13 deg is avoided: we use
        // the fov directly as the half-width so scene files stay intuitive).
        double hw, hh;
        if (aspect >= 1.0) {
            hw = half;
            hh = half / aspect;
        } else {
            hw = half * aspect;
            hh = half;
        }
        *uMin = -hw;
        *uMax = hw;
        *vMin = -hh;
        *vMax = hh;
    }
    *orthoScale = std::max(*uMax - *uMin, *vMax - *vMin);
}

void SetBasis(Camera* c, Point3 pos, Point3 look, Vec3 up) {
    c->position = pos;
    c->forward = Normalize(look - pos);
    c->CameraToWorld = Transform::LookAt(pos, look, up).GetInverse();
}

}  // namespace

void Camera::ConfigurePerspective(double fovDegrees, const Point3& pos, Point3 look,
                                  Vec3 up, const Film& film) {
    type = ProjectionType::Perspective;
    fov = fovDegrees;
    lensRadius = 0.0;
    SetBasis(this, pos, look, up);
    WindowFromFov(film, fovDegrees, true, &uMin, &uMax, &vMin, &vMax, &orthoScale);
    lastSetup = Setup::Perspective;
    setupFov = fovDegrees;
    setupAperture = 0.0;
    setupFocus = 0.0;
    setupPos = pos;
    setupLook = look;
    setupUp = up;
    setupFilm = film;
}

void Camera::ConfigureOrthographic(double orthoWidth, const Point3& pos, Point3 look,
                                   Vec3 up, const Film& film) {
    type = ProjectionType::Orthographic;
    lensRadius = 0.0;
    lastSetup = Setup::Orthographic;
    setupOrthoWidth = orthoWidth;
    setupPos = pos;
    setupLook = look;
    setupUp = up;
    setupFilm = film;
    fov = Degrees(2.0 * std::atan(0.5 * orthoWidth));
    SetBasis(this, pos, look, up);
    // Interpret orthoWidth as the world-space extent across the larger axis and
    // derive the window directly; the fov field is kept only for reporting.
    double aspect = (film.pixelAspect * film.xResolution) / film.yResolution;
    double hw, hh;
    if (aspect >= 1.0) {
        hw = 0.5 * orthoWidth;
        hh = hw / aspect;
    } else {
        hh = 0.5 * orthoWidth;
        hw = hh * aspect;
    }
    uMin = -hw; uMax = hw; vMin = -hh; vMax = hh;
    orthoScale = orthoWidth;
}

void Camera::ConfigureThinLens(double apertureRadius, double focusDistance,
                               double fovDegrees, const Point3& pos, Point3 look, Vec3 up,
                               const Film& film) {
    ConfigurePerspective(fovDegrees, pos, look, up, film);
    lensRadius = apertureRadius;
    lastSetup = Setup::ThinLens;
    setupFov = fovDegrees;
    setupAperture = apertureRadius;
    setupFocus = focusDistance;
    this->focusDistance =
        focusDistance > 0 ? focusDistance : Distance(pos, look);
}

Vec3 Camera::CameraRight() const {
    // right = forward x up, so that a camera looking down -z with up = +y has
    // right = +x. (Using up x forward would mirror the image horizontally.)
    Vec3 up(0, 1, 0);
    if (std::fabs(Dot(forward, up)) > 0.999) up = Vec3(0, 0, 1);
    return Normalize(Cross(forward, up));
}

Vec3 Camera::CameraUp() const { return Normalize(Cross(CameraRight(), forward)); }

void Camera::Reconfigure() {
    switch (lastSetup) {
        case Setup::Perspective:
            ConfigurePerspective(setupFov, setupPos, setupLook, setupUp, setupFilm);
            break;
        case Setup::Orthographic:
            ConfigureOrthographic(setupOrthoWidth, setupPos, setupLook, setupUp, setupFilm);
            break;
        case Setup::ThinLens:
            ConfigureThinLens(setupAperture, setupFocus, setupFov, setupPos, setupLook,
                              setupUp, setupFilm);
            break;
        case Setup::None:
            break;
    }
}

Ray Camera::GenerateRay(double sx, double sy, Point2 lensSample) const {
    double px = uMin + sx * (uMax - uMin);
    double py = vMin + sy * (vMax - vMin);
    Vec3 right = CameraRight();
    Vec3 upv = CameraUp();

    if (type == ProjectionType::Orthographic) {
        // Parallel rays: the screen window is already in world units, so the
        // ray origin is offset in the plane through `position` and the direction
        // is simply `forward` (no normalisation changes t from world distance).
        return Ray(position + right * px + upv * py, forward);
    }

    // Perspective / thin lens: the screen window lives at distance 1.
    Vec3 dir = Normalize(forward + right * px + upv * py);
    if (lensRadius <= 0) return Ray(position, dir);

    // Thin lens: choose a point on the aperture disk and aim it at the same
    // screen position on the focal plane.
    double r = lensRadius * std::sqrt(lensSample.x);
    double theta = 2.0 * kPi * lensSample.y;
    Vec3 offset = (right * std::cos(theta) + upv * std::sin(theta)) * r;
    Point3 lensOrigin = position + offset;
    Point3 focusPoint = position + dir * focusDistance;
    return Ray(lensOrigin, Normalize(focusPoint - lensOrigin));
}

}  // namespace cgr
