#pragma once
// Triangle mesh with per-vertex normals and UVs, plus a Wavefront OBJ reader
// written against the standard library only.
#include <memory>
#include <string>
#include <vector>

#include "scene/shape.h"

namespace cgr {

class TriangleMesh {
  public:
    struct Face {
        int v[3] = {0, 0, 0};  // vertex indices
        int vt[3] = {0, 0, 0}; // uv indices
        int vn[3] = {0, 0, 0}; // normal indices
    };

    std::string name;
    std::vector<Point3> P;
    std::vector<Point2> UVs;
    std::vector<Normal> N;
    std::vector<Face> faces;

    AABB GetBounds() const;
    size_t NumTriangles() const { return faces.size(); }

    // Builds Triangle objects with the mesh transform already baked into their
    // vertex data, so BVH leaves can point straight at them.
    std::vector<std::shared_ptr<Triangle>> BuildTriangles(const Transform& toWorld) const;

    // Recomputes per-vertex normals by area-weighted averaging of face normals.
    void RecomputeNormals();
};

using TriangleMeshPtr = std::shared_ptr<TriangleMesh>;

// Parses a .obj file. Returns nullptr and fills `err` on failure.
TriangleMeshPtr LoadOBJ(const std::string& path, std::string* err);

}  // namespace cgr
