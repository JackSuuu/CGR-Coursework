#pragma once
// Axis-aligned bounding box, shared by the primitives and the BVH.
#include <limits>
#include <vector>

#include "core/vec.h"

namespace cgr {

struct AABB {
    Point3 pMin = {kInf, kInf, kInf};
    Point3 pMax = {-kInf, -kInf, -kInf};

    AABB() = default;
    AABB(Point3 lo, Point3 hi) : pMin(lo), pMax(hi) {}

    Point3 Centroid() const { return (pMin + pMax) * 0.5; }
    Point3 Diagonal() const { return pMax - pMin; }
    double SurfaceArea() const {
        Vec3 d = Diagonal();
        if (d.x < 0 || d.y < 0 || d.z < 0) return 0;
        return 2.0 * (d.x * d.y + d.y * d.z + d.z * d.x);
    }
    int MaxExtentDim() const { return MaxComponentDim(Diagonal()); }
    bool Valid() const { return pMax.x >= pMin.x && pMax.y >= pMin.y && pMax.z >= pMin.z; }

    void Expand(const Point3& p) {
        pMin = Min(pMin, p);
        pMax = Max(pMax, p);
    }
    void Expand(const AABB& b) {
        pMin = Min(pMin, b.pMin);
        pMax = Max(pMax, b.pMax);
    }
    AABB Union(const AABB& b) const {
        return AABB(Min(pMin, b.pMin), Max(pMax, b.pMax));
    }
    AABB Intersect(const AABB& b) const {
        return AABB(Max(pMin, b.pMin), Min(pMax, b.pMax));
    }
    AABB Padded(double eps) const {
        Vec3 d = Diagonal() * 0.5;
        Vec3 pad = {std::max(eps, d.x * 1e-3), std::max(eps, d.y * 1e-3),
                    std::max(eps, d.z * 1e-3)};
        return AABB(pMin - pad, pMax + pad);
    }

    bool Contains(Point3 p) const {
        return p.x >= pMin.x && p.x <= pMax.x && p.y >= pMin.y && p.y <= pMax.y &&
               p.z >= pMin.z && p.z <= pMax.z;
    }
    bool Intersects(const AABB& b) const {
        return pMin.x <= b.pMax.x && pMax.x >= b.pMin.x && pMin.y <= b.pMax.y &&
               pMax.y >= b.pMin.y && pMin.z <= b.pMax.z && pMax.z >= b.pMin.z;
    }
    Point3 Offset(Point3 p) const {
        Vec3 d = Diagonal();
        return {(p.x - pMin.x) / d.x, (p.y - pMin.y) / d.y, (p.z - pMin.z) / d.z};
    }
    Point3 Lerp(Point3 t) const {
        return {pMin.x + t.x * (pMax.x - pMin.x), pMin.y + t.y * (pMax.y - pMin.y),
                pMin.z + t.z * (pMax.z - pMin.z)};
    }
};

}  // namespace cgr
