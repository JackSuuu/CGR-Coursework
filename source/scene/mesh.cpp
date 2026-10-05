#include "scene/mesh.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace cgr {

AABB TriangleMesh::GetBounds() const {
    AABB b;
    for (const auto& p : P) b.Expand(p);
    return b.Padded(0.0);
}

void TriangleMesh::RecomputeNormals() {
    N.assign(P.size(), Normal(0, 0, 0));
    for (const auto& f : faces) {
        Point3 p0 = P[f.v[0]], p1 = P[f.v[1]], p2 = P[f.v[2]];
        // Area-weighted: the unnormalised cross product is twice the area.
        Vec3 fn = 2.0 * Cross(p1 - p0, p2 - p0);
        for (int i = 0; i < 3; ++i) N[f.v[i]] += fn;
    }
    for (auto& n : N) n = Normalize(n);
}

std::vector<std::shared_ptr<Triangle>> TriangleMesh::BuildTriangles(
    const Transform& toWorld) const {
    std::vector<std::shared_ptr<Triangle>> tris;
    tris.reserve(faces.size());
    for (const auto& f : faces) {
        auto tri = std::make_shared<Triangle>();
        tri->p[0] = toWorld.TransformPoint(P[f.v[0]]);
        tri->p[1] = toWorld.TransformPoint(P[f.v[1]]);
        tri->p[2] = toWorld.TransformPoint(P[f.v[2]]);
        tri->e1 = tri->p[1] - tri->p[0];
        tri->e2 = tri->p[2] - tri->p[0];
        tri->n = Normalize(Cross(tri->e1, tri->e2));
        for (int i = 0; i < 3; ++i) {
            if (!UVs.empty() && f.vt[i] >= 0 &&
                f.vt[i] < static_cast<int>(UVs.size()))
                tri->uv[i] = UVs[f.vt[i]];
            else
                tri->uv[i] = Point2(0, 0);
            if (!N.empty() && f.vn[i] >= 0 && f.vn[i] < static_cast<int>(N.size()))
                tri->sn[i] = N[f.vn[i]];
        }
        tri->hasVertexNormals = !N.empty();
        // Baked into world space, so the triangle's own transform is identity.
        tri->SetTransform(Transform());
        tris.push_back(tri);
    }
    return tris;
}

namespace {
// OBJ indices may be negative (relative to the end) and may be v/vt/vn triples.
int ResolveIndex(long v, size_t count) {
    if (v > 0) return static_cast<int>(v - 1);
    if (v < 0) return static_cast<int>(static_cast<long>(count) + v);
    return -1;
}
}  // namespace

TriangleMeshPtr LoadOBJ(const std::string& path, std::string* err) {
    std::ifstream f(path);
    if (!f) {
        if (err) *err = "cannot open mesh file '" + path + "'";
        return nullptr;
    }
    auto mesh = std::make_shared<TriangleMesh>();
    mesh->name = path;
    std::string line;
    int lineNo = 0;
    bool anyNormal = false;
    while (std::getline(f, line)) {
        ++lineNo;
        // Strip comments and trailing whitespace.
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        std::istringstream is(line);
        std::string tok;
        if (!(is >> tok)) continue;
        if (tok == "v") {
            double x, y, z;
            if (!(is >> x >> y >> z)) {
                if (err)
                    *err = path + ":" + std::to_string(lineNo) +
                           ": malformed vertex (expected 3 floats)";
                return nullptr;
            }
            mesh->P.push_back(Point3(x, y, z));
        } else if (tok == "vn") {
            double x, y, z;
            if (!(is >> x >> y >> z)) {
                if (err)
                    *err = path + ":" + std::to_string(lineNo) + ": malformed normal";
                return nullptr;
            }
            mesh->N.push_back(Normal(x, y, z));
            anyNormal = true;
        } else if (tok == "vt") {
            double u, v;
            if (!(is >> u >> v)) {
                if (err) *err = path + ":" + std::to_string(lineNo) + ": malformed texture coord";
                return nullptr;
            }
            // OBJ v is bottom-up; pbrt textures are top-down.
            mesh->UVs.push_back(Point2(u, 1 - v));
        } else if (tok == "f") {
            std::vector<std::string> corners;
            std::string c;
            while (is >> c) corners.push_back(c);
            if (corners.size() < 3) {
                if (err)
                    *err = path + ":" + std::to_string(lineNo) + ": face has " +
                           std::to_string(corners.size()) + " vertices (need >= 3)";
                return nullptr;
            }
            // Fan-triangulate n-gons.
            for (size_t k = 2; k < corners.size(); ++k) {
                const std::string* tri[3] = {&corners[0], &corners[k - 1], &corners[k]};
                TriangleMesh::Face face;
                for (int i = 0; i < 3; ++i) {
                    // Formats: v, v/vt, v//vn, v/vt/vn
                    std::string s = *tri[i];
                    size_t s1 = s.find('/');
                    if (s1 == std::string::npos) {
                        face.v[i] = ResolveIndex(std::strtol(s.c_str(), nullptr, 10), mesh->P.size());
                    } else {
                        size_t s2 = s.find('/', s1 + 1);
                        face.v[i] = ResolveIndex(
                            std::strtol(s.substr(0, s1).c_str(), nullptr, 10), mesh->P.size());
                        std::string uvPart =
                            (s2 == std::string::npos) ? s.substr(s1 + 1) : s.substr(s1 + 1, s2 - s1 - 1);
                        if (!uvPart.empty())
                            face.vt[i] = ResolveIndex(
                                std::strtol(uvPart.c_str(), nullptr, 10), mesh->UVs.size());
                        if (s2 != std::string::npos) {
                            std::string nPart = s.substr(s2 + 1);
                            if (!nPart.empty())
                                face.vn[i] = ResolveIndex(
                                    std::strtol(nPart.c_str(), nullptr, 10), mesh->N.size());
                        }
                    }
                    if (face.v[i] < 0 || face.v[i] >= static_cast<int>(mesh->P.size())) {
                        if (err)
                            *err = path + ":" + std::to_string(lineNo) +
                                   ": face references vertex index out of range";
                        return nullptr;
                    }
                }
                mesh->faces.push_back(face);
            }
        }
    }
    if (mesh->P.empty()) {
        if (err) *err = "mesh file '" + path + "' contains no vertices";
        return nullptr;
    }
    if (mesh->faces.empty()) {
        if (err) *err = "mesh file '" + path + "' contains no faces";
        return nullptr;
    }
    if (!anyNormal) mesh->RecomputeNormals();
    return mesh;
}

}  // namespace cgr
