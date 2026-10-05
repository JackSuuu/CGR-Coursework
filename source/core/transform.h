#pragma once
// 4x4 column-major affine transform, matching pbrt's Transform semantics:
//   v' = M * v (vector),  p' = M * p with the implicit w = 1 for points.
#include <memory>

#include "core/vec.h"

namespace cgr {

class Transform {
  public:
    // Identity.
    Transform() = default;

    static Transform Translate(Vec3 delta);
    static Transform Scale(double x, double y, double z);
    static Transform Scale(double s);
    static Transform RotateX(double deg);
    static Transform RotateY(double deg);
    static Transform RotateZ(double deg);
    static Transform Rotate(Point3 axis, double deg);
    // LookAt builds a world->camera transform (pbrt's LookAt).
    static Transform LookAt(Point3 eye, Point3 look, Vec3 up);
    static Transform FromMatrix(const double m[4][4]);

    // Compose: returns a * b, i.e. b applied first.
    Transform operator*(const Transform& b) const;
    Transform GetInverse() const;
    // Transpose of the 3x3 block; correct only for rigid transforms.
    Transform GetTranspose() const;

    Point3 TransformPoint(Point3 p) const;
    Vec3 TransformVector(Vec3 v) const;
    Normal TransformNormal(Normal n) const;
    // Direction of a ray is affected by translation too; pbrt keeps the same
    // transform for o and d but that shifts the origin, so we transform the
    // origin and then re-derive the direction from the transformed far point.
    Ray TransformRay(const Ray& r) const;

    bool IsIdentity() const { return m[0][0] == 1 && m[1][1] == 1 && m[2][2] == 1 && m[0][3] == 0 && m[1][3] == 0 && m[2][3] == 0; }
    void Swap(Transform& t);
    const double* Matrix() const { return &m[0][0]; }

  private:
    double m[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
};

}  // namespace cgr
