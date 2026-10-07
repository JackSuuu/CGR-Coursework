#include "accel/bvh.h"

#include <algorithm>
#include <numeric>

#include "core/logger.h"
#include "core/timer.h"
#include "scene/shape.h"

namespace cgr {

BVH::~BVH() = default;

bool BVH::Enabled() {
    static const char* env = std::getenv("CGR_NO_BVH");
    return !(env && env[0] && env[0] != '0');
}

void BVH::Clear() {
    nodes_.clear();
    prims_.clear();
    shapes_ = nullptr;
    nLeaves_ = maxDepth_ = 0;
}

void BVH::BoundsFromPrims(int start, int end, AABB* b, AABB* cb) const {
    AABB bb;
    AABB cbb;
    for (int i = start; i < end; ++i) {
        const ShapePtr& s = (*shapes_)[prims_[i]];
        bb.Expand(s->GetBounds());
        cbb.Expand(s->GetBounds().Centroid());
    }
    *b = bb;
    *cb = cbb;
}

int BVH::BuildRecursive(int start, int end, std::vector<int>& indices, int depth) {
    int nodeIndex = static_cast<int>(nodes_.size());
    nodes_.push_back(Node());
    maxDepth_ = std::max(maxDepth_, static_cast<size_t>(depth));

    AABB bounds, centroidBounds;
    BoundsFromPrims(start, end, &bounds, &centroidBounds);

    int n = end - start;
    if (n == 1) {
        nodes_[nodeIndex].bounds = bounds;
        nodes_[nodeIndex].leftFirst = start;
        nodes_[nodeIndex].nPrims = 1;
        ++nLeaves_;
        return nodeIndex;
    }

    // Binned surface-area-heuristic split along the widest centroid extent.
    // Falls back to the median when the centroids coincide (a classic
    // degenerate case the Module 3 worst-case scene relies on).
    Vec3 ce = centroidBounds.Diagonal();
    int dim = MaxComponentDim(ce);
    int mid = start + n / 2;
    if (ce[dim] > 1e-12) {
        constexpr int kBins = 16;
        struct Bin {
            AABB b;
            int n = 0;
        };
        Bin bins[kBins];
        double scale = kBins / ce[dim];
        for (int i = start; i < end; ++i) {
            int bIdx = static_cast<int>(((*shapes_)[prims_[i]]->GetBounds().Centroid()[dim] -
                                         centroidBounds.pMin[dim]) *
                                        scale);
            bIdx = Clamp(bIdx, 0, kBins - 1);
            bins[bIdx].n++;
            bins[bIdx].b.Expand((*shapes_)[prims_[i]]->GetBounds());
        }
        double cost[kBins - 1];
        AABB acc;
        int accN = 0;
        for (int i = 0; i < kBins - 1; ++i) {
            acc.Expand(bins[i].b);
            accN += bins[i].n;
            cost[i] = accN * acc.SurfaceArea();
        }
        AABB racc;
        int raccN = 0;
        for (int i = kBins - 1; i >= 1; --i) {
            racc.Expand(bins[i].b);
            raccN += bins[i].n;
            cost[i - 1] += raccN * racc.SurfaceArea();
        }
        int bestSplit = -1;
        double bestCost = kInf;
        for (int i = 0; i < kBins - 1; ++i) {
            if (bins[i].n == 0 || bins[i + 1].n == 0) continue;
            if (cost[i] < bestCost) {
                bestCost = cost[i];
                bestSplit = i;
            }
        }
        if (bestSplit >= 0) {
            auto it = std::partition(prims_.begin() + start, prims_.begin() + end,
                                     [&](int pi) {
                                         double c =
                                             (*shapes_)[pi]->GetBounds().Centroid()[dim];
                                         int bi = static_cast<int>(
                                             (c - centroidBounds.pMin[dim]) * scale);
                                         bi = Clamp(bi, 0, kBins - 1);
                                         return bi <= bestSplit;
                                     });
            mid = static_cast<int>(it - prims_.begin());
        }
    }
    if (mid == start || mid == end) {
        // Degenerate split: fall back to the median so the recursion ends.
        std::nth_element(prims_.begin() + start, prims_.begin() + mid,
                         prims_.begin() + end,
                         [&](int a, int b) {
                             return (*shapes_)[a]->GetBounds().Centroid()[dim] <
                                    (*shapes_)[b]->GetBounds().Centroid()[dim];
                         });
        mid = start + n / 2;
    }

    int left = BuildRecursive(start, mid, indices, depth + 1);
    int right = BuildRecursive(mid, end, indices, depth + 1);
    nodes_[nodeIndex].bounds = nodes_[left].bounds.Union(nodes_[right].bounds);
    nodes_[nodeIndex].leftFirst = left;
    nodes_[nodeIndex].rightFirst = right;
    nodes_[nodeIndex].nPrims = 0;
    return nodeIndex;
}

bool BVH::Build(const std::vector<ShapePtr>& shapes) {
    Clear();
    if (shapes.empty()) return false;
    Clock::time_point t0 = Clock::now();
    shapes_ = &shapes;
    prims_.resize(shapes.size());
    nodes_.reserve(shapes.size() * 2 + 1);
    std::iota(prims_.begin(), prims_.end(), 0);
    std::vector<int> dummy;
    // A very large leaf size keeps the tree shallow, which is the right choice
    // for a scene of a few thousand small triangles.
    BuildRecursive(0, static_cast<int>(shapes.size()), dummy, 0);
    buildSeconds_ = ElapsedSeconds(t0, Clock::now());
    Profiler::Instance().Add("bvh-build", buildSeconds_);
    Logger::Instance().Info("BVH: " + std::to_string(nodes_.size()) + " nodes, " +
                            std::to_string(nLeaves_) + " leaves, depth " +
                            std::to_string(maxDepth_) + ", built in " +
                            std::to_string(buildSeconds_) + " s");
    return true;
}

bool BVH::Intersect(const Ray& r, double tMax, double* tHit, int* hitIndex) const {
    if (nodes_.empty() || !shapes_) return false;
    ++rayCount_;
    bool hit = false;
    int stack[128];
    int sp = 0;
    stack[sp++] = 0;
    double t = tMax;
    while (sp > 0) {
        int ni = stack[--sp];
        const Node& n = nodes_[ni];
        double tNear, tFar;
        if (!SlabTest(&r.o.x, &r.d.x, &n.bounds.pMin.x, &n.bounds.pMax.x, 0.0, t,
                      &tNear, &tFar))
            continue;
        if (n.nPrims > 0) {
            for (int i = 0; i < n.nPrims; ++i) {
                int pi = prims_[n.leftFirst + i];
                ++primTestCount_;
                double th;
                if ((*shapes_)[pi]->IntersectRay(r, t, &th) && th < t) {
                    t = th;
                    if (hitIndex) *hitIndex = pi;
                    hit = true;
                }
            }
        } else if (sp + 2 <= 128) {
            stack[sp++] = n.leftFirst;
            stack[sp++] = n.rightFirst;
        }
    }
    if (hit) *tHit = t;
    return hit;
}

bool BVH::IntersectAny(const Ray& r, double tMax) const {
    if (nodes_.empty() || !shapes_) return false;
    ++rayCount_;
    int stack[128];
    int sp = 0;
    stack[sp++] = 0;
    while (sp > 0) {
        int ni = stack[--sp];
        const Node& n = nodes_[ni];
        double tNear, tFar;
        if (!SlabTest(&r.o.x, &r.d.x, &n.bounds.pMin.x, &n.bounds.pMax.x, 0.0, tMax,
                      &tNear, &tFar))
            continue;
        if (n.nPrims > 0) {
            for (int i = 0; i < n.nPrims; ++i) {
                ++primTestCount_;
                double th;
                if ((*shapes_)[prims_[n.leftFirst + i]]->IntersectRay(r, tMax, &th))
                    return true;
            }
        } else if (sp + 2 <= 128) {
            stack[sp++] = n.leftFirst;
            stack[sp++] = n.rightFirst;
        }
    }
    return false;
}

}  // namespace cgr
