#pragma once
// Scene container and the parameter dictionary the parser fills in.
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/logger.h"
#include "core/sampler.h"
#include "core/texture.h"
#include "scene/camera.h"
#include "scene/light.h"
#include "scene/material.h"
#include "scene/mesh.h"
#include "scene/shape.h"

namespace cgr {

// A parsed "Type name { ... }" block.
using ParamList = std::map<std::string, std::vector<std::string>>;

enum class IntegratorType { Whitted, Distributed };

struct Scene {
    Camera camera;
    Film film;
    std::vector<ShapePtr> shapes;
    std::vector<LightPtr> lights;
    std::vector<MaterialPtr> materials;  // owned; shapes point into this
    std::string integrator = "whitted";
    // DRT parameters; overridable from the scene file.
    int maxDepth = 3;         // MD: maximum ray-tree depth
    int branchingFactor = 4;  // BF: rays per scattering event
    int spp = 4;               // light samples per pixel
    SamplerType sampler = SamplerType::Uniform;
    // Tone mapping.
    double whitePoint = 4.0;
    bool perChannelToneMap = true;
    bool gammaCorrect = true;
    // Hard cap so a runaway scene cannot exhaust memory.
    int maxPrimitives = 5000000;
    // Stats for the log.
    int numPrimitives = 0;
    int numTriangles = 0;
    size_t numMeshFiles = 0;
};

// Owns a default material so shapes that do not declare one still render.
MaterialPtr DefaultMaterial();

}  // namespace cgr
