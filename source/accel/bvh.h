#pragma once
// Bounding volume hierarchy over axis-aligned boxes.
//
// This is the Module 3 deliverable, but it is needed already in Module 1 to keep
// mesh rendering interactive, so it lives behind an interface the integrators
// can bypass. Setting `enabled = false` falls back to brute-force intersection,
// which is exactly the "without acceleration" measurement the spec asks for.
#include <memory>
#include <vector>

#include "core/aabb.h"
#include "core/vec.h"
#include "scene/shape.h"

namespace cgr {

class BVH {
  public:
    BVH() = default;
    ~BVH();
    // Builds the hierarchy over [0, shapes.size()). Returns false if there is
    // nothing to build.
    bool Build(const std::vector<ShapePtr>& shapes);
    void Clear();

    bool Intersect(const Ray& r, double tMax, double* tHit, int* hitIndex) const;
    bool IntersectAny(const Ray& r, double tMax) const;

    size_t NumNodes() const { return nodes_.size(); }
    size_t NumLeaves() const { return nLeaves_; }
    size_t MaxDepth() const { return maxDepth_; }
    size_t NumPrimitives() const { return prims_.size(); }
    // Ray / primitive-test counters, reported in the log for the Module 3
    // analysis.
    long long RayCount() const { return rayCount_; }
    long long PrimTestCount() const { return primTestCount_; }
    double BuildSeconds() const { return buildSeconds_; }
    void ResetCounters() const {
        rayCount_ = primTestCount_ = 0;
    }

    // Global switch: when false both integrators fall back to a linear scan.
    static bool Enabled();

  private:
    struct Node {
        AABB bounds;
        // Interior nodes store both child indices explicitly. Assuming the right
        // child sits at leftFirst + 1 is wrong as soon as the left child is
        // itself an interior node, because the whole left subtree is allocated
        // before the right one.
        int leftFirst = -1;   // interior: left child index; leaf: first prim
        int rightFirst = -1;  // interior: right child index
        int nPrims = 0;       // 0 for interior nodes
    };

    int BuildRecursive(int start, int end, std::vector<int>& indices, int depth);
    void BoundsFromPrims(int start, int end, AABB* b, AABB* centroidBounds) const;

    std::vector<Node> nodes_;
    std::vector<int> prims_;
    const std::vector<ShapePtr>* shapes_ = nullptr;
    size_t nLeaves_ = 0, maxDepth_ = 0;
    double buildSeconds_ = 0;
    mutable long long rayCount_ = 0, primTestCount_ = 0;
};

}  // namespace cgr
