#include "core/transform.h"

#include <cmath>

namespace cgr {

Transform Transform::Translate(Vec3 delta) {
    Transform t;
    t.m[0][3] = delta.x;
    t.m[1][3] = delta.y;
    t.m[2][3] = delta.z;
    return t;
}

Transform Transform::Scale(double x, double y, double z) {
    Transform t;
    t.m[0][0] = x;
    t.m[1][1] = y;
    t.m[2][2] = z;
    return t;
}

Transform Transform::Scale(double s) { return Scale(s, s, s); }

Transform Transform::RotateX(double deg) {
    double r = Radians(deg), c = std::cos(r), s = std::sin(r);
    Transform t;
    t.m[1][1] = c;  t.m[1][2] = -s;
    t.m[2][1] = s;  t.m[2][2] = c;
    return t;
}

Transform Transform::RotateY(double deg) {
    double r = Radians(deg), c = std::cos(r), s = std::sin(r);
    Transform t;
    t.m[0][0] = c;  t.m[0][2] = s;
    t.m[2][0] = -s; t.m[2][2] = c;
    return t;
}

Transform Transform::RotateZ(double deg) {
    double r = Radians(deg), c = std::cos(r), s = std::sin(r);
    Transform t;
    t.m[0][0] = c;  t.m[0][1] = -s;
    t.m[1][0] = s;  t.m[1][1] = c;
    return t;
}

Transform Transform::Rotate(Point3 axis, double deg) {
    // Rodrigues' rotation formula, pbrt's Rotation().
    Vec3 a = Normalize(axis);
    double rad = Radians(deg);
    double c = std::cos(rad), s = std::sin(rad);
    Transform t;
    t.m[0][0] = a.x * a.x + (1 - a.x * a.x) * c;
    t.m[0][1] = a.x * a.y * (1 - c) - a.z * s;
    t.m[0][2] = a.x * a.z * (1 - c) + a.y * s;
    t.m[0][3] = 0;
    t.m[1][0] = a.x * a.y * (1 - c) + a.z * s;
    t.m[1][1] = a.y * a.y + (1 - a.y * a.y) * c;
    t.m[1][2] = a.y * a.z * (1 - c) - a.x * s;
    t.m[1][3] = 0;
    t.m[2][0] = a.x * a.z * (1 - c) - a.y * s;
    t.m[2][1] = a.y * a.z * (1 - c) + a.x * s;
    t.m[2][2] = a.z * a.z + (1 - a.z * a.z) * c;
    t.m[2][3] = 0;
    return t;
}

Transform Transform::LookAt(Point3 eye, Point3 look, Vec3 up) {
    Vec3 dir = Normalize(look - eye);
    Vec3 right = Cross(dir, Normalize(up));
    if (LengthSquared(right) < 1e-12) {
        // Degenerate up vector; pick any orthogonal axis.
        Vec3 alt = std::fabs(dir.y) < 0.9 ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
        right = Cross(dir, alt);
    }
    right = Normalize(right);
    Vec3 newUp = Cross(right, dir);
    Transform t;
    t.m[0][0] = right.x; t.m[0][1] = right.y; t.m[0][2] = right.z; t.m[0][3] = -Dot(right, eye);
    t.m[1][0] = newUp.x; t.m[1][1] = newUp.y; t.m[1][2] = newUp.z; t.m[1][3] = -Dot(newUp, eye);
    t.m[2][0] = -dir.x;  t.m[2][1] = -dir.y;  t.m[2][2] = -dir.z;  t.m[2][3] = Dot(dir, eye);
    return t;
}

Transform Transform::FromMatrix(const double mat[4][4]) {
    Transform t;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) t.m[i][j] = mat[i][j];
    return t;
}

Transform Transform::operator*(const Transform& b) const {
    Transform r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            double sum = 0;
            for (int k = 0; k < 4; ++k) sum += m[i][k] * b.m[k][j];
            r.m[i][j] = sum;
        }
    return r;
}

void Transform::Swap(Transform& t) {
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) std::swap(m[i][j], t.m[i][j]);
}

Transform Transform::GetInverse() const {
    // General 4x4 inverse via Gauss-Jordan with partial pivoting. Transforms in
    // a scene are small and few, so clarity beats a specialised closed form.
    double a[4][8];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) a[i][j] = m[i][j];
        for (int j = 0; j < 4; ++j) a[i][4 + j] = (i == j) ? 1.0 : 0.0;
    }
    for (int col = 0; col < 4; ++col) {
        int piv = col;
        for (int r = col + 1; r < 4; ++r)
            if (std::fabs(a[r][col]) > std::fabs(a[piv][col])) piv = r;
        if (std::fabs(a[piv][col]) < 1e-12) return Transform();  // singular
        if (piv != col)
            for (int j = 0; j < 8; ++j) std::swap(a[col][j], a[piv][j]);
        double inv = 1.0 / a[col][col];
        for (int j = 0; j < 8; ++j) a[col][j] *= inv;
        for (int r = 0; r < 4; ++r) {
            if (r == col) continue;
            double f = a[r][col];
            if (f == 0.0) continue;
            for (int j = 0; j < 8; ++j) a[r][j] -= f * a[col][j];
        }
    }
    Transform out;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) out.m[i][j] = a[i][4 + j];
    return out;
}

Transform Transform::GetTranspose() const {
    Transform t;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) t.m[i][j] = m[j][i];
    return t;
}

Point3 Transform::TransformPoint(Point3 p) const {
    double x = m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + m[0][3];
    double y = m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + m[1][3];
    double z = m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z + m[2][3];
    double w = m[3][0] * p.x + m[3][1] * p.y + m[3][2] * p.z + m[3][3];
    if (w != 1.0 && w != 0.0) return {x / w, y / w, z / w};
    return {x, y, z};
}

Vec3 Transform::TransformVector(Vec3 v) const {
    return {m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z};
}

Normal Transform::TransformNormal(Normal n) const {
    // Inverse transpose preserves perpendicularity under non-uniform scale.
    Transform inv = GetInverse();
    return Normalize(Vec3(inv.m[0][0] * n.x + inv.m[1][0] * n.y + inv.m[2][0] * n.z,
                          inv.m[0][1] * n.x + inv.m[1][1] * n.y + inv.m[2][1] * n.z,
                          inv.m[0][2] * n.x + inv.m[1][2] * n.y + inv.m[2][2] * n.z));
}

Ray Transform::TransformRay(const Ray& r) const {
    // Affine transforms preserve the SAME ray parameter t only if the
    // transformed direction is not normalised. Camera rays are unit length;
    // object-space rays generally are not.
    return Ray(TransformPoint(r.o), TransformVector(r.d), r.tMax);
}

}  // namespace cgr
