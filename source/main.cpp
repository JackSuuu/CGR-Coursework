// CGR26_renderer entry point.
//
// Usage:
//   CGR26_renderer <scene.pbrt> [options]
//
// The integrator is chosen by the scene's `Integrator` directive, so the same
// executable serves both the Whitted-style and the distributed ray tracer.
// Every run writes the image and a matching .log into the output directory.
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "accel/bvh.h"
#include "core/image.h"
#include "core/logger.h"
#include "core/timer.h"
#include "integrator/integrator.h"
#include "scene/parser.h"
#include "scene/scene.h"

namespace {

void PrintUsage(const char* exe) {
    std::cout
        << "CGR26_renderer - physically based ray tracer (CGR 2026)\n\n"
        << "Usage: " << exe << " <scene.pbrt> [options]\n\n"
        << "Options:\n"
        << "  -o, --output <dir>     directory for the image and log "
           "(default: output)\n"
        << "  -n, --name <stem>     output file stem (default: the scene stem)\n"
        << "  -f, --format <fmt>    ppm | png (default: png)\n"
        << "  -w, --width <n>       override the film x resolution\n"
        << "  -H, --height <n>      override the film y resolution\n"
        << "  -s, --spp <n>         override samples per pixel\n"
        << "  -d, --depth <n>       override the maximum ray-tree depth\n"
        << "  -b, --bf <n>          override the branching factor\n"
        << "      --sampler <name>  uniform | grid | halton | haltonjittered\n"
        << "      --no-bvh          disable the BVH (brute-force timing runs)\n"
        << "      --tonemap <name>  reinhard | reinhardluminance\n"
        << "      --white <v>       Reinhard white point\n"
        << "      --aperture <r>    thin-lens aperture radius (0 = pinhole)\n"
        << "      --focusdistance <d> thin-lens focal distance\n"
        << "      --fov <deg>       override the field of view\n"
        << "  -q, --quiet           do not echo the log to stdout\n"
        << "  -v, --version         print the version and exit\n"
        << "  -h, --help            print this message and exit\n";
}

std::string StemOf(const std::string& path) {
    size_t slash = path.find_last_of('/');
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    size_t dot = base.find_last_of('.');
    if (dot != std::string::npos && dot > 0) base = base.substr(0, dot);
    return base;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace cgr;

    if (argc < 2) {
        PrintUsage(argv[0]);
        return 2;
    }
    std::string scenePath;
    std::string outDir = "output";
    std::string outStem;
    ImageFormat format = ImageFormat::PNG;
    int overrideW = 0, overrideH = 0, overrideSpp = 0, overrideDepth = 0, overrideBF = 0;
    bool forceNoBvh = false;
    std::string overrideSampler, overrideToneMap;
    double overrideWhite = 0, overrideAperture = -1, overrideFocus = -1, overrideFov = -1;
    bool quiet = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto needValue = [&](const char* what) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "error: " << what << " requires a value\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "-h" || a == "--help") {
            PrintUsage(argv[0]);
            return 0;
        }
        if (a == "-v" || a == "--version") {
            std::cout << "CGR26_renderer 1.0 (CGR 2026 coursework framework)\n";
            return 0;
        }
        if (a == "-o" || a == "--output") {
            outDir = needValue("--output");
        } else if (a == "-n" || a == "--name") {
            outStem = needValue("--name");
        } else if (a == "-f" || a == "--format") {
            std::string f = needValue("--format");
            if (f == "ppm" || f == "PPM")
                format = ImageFormat::PPM;
            else if (f == "png" || f == "PNG")
                format = ImageFormat::PNG;
            else {
                std::cerr << "error: unknown image format '" << f
                          << "' (expected ppm or png)\n";
                return 2;
            }
        } else if (a == "-w" || a == "--width") {
            overrideW = std::atoi(needValue("--width").c_str());
        } else if (a == "-H" || a == "--height") {
            overrideH = std::atoi(needValue("--height").c_str());
        } else if (a == "-s" || a == "--spp") {
            overrideSpp = std::atoi(needValue("--spp").c_str());
        } else if (a == "-d" || a == "--depth") {
            overrideDepth = std::atoi(needValue("--depth").c_str());
        } else if (a == "-b" || a == "--bf") {
            overrideBF = std::atoi(needValue("--bf").c_str());
        } else if (a == "--sampler") {
            overrideSampler = needValue("--sampler");
        } else if (a == "--tonemap") {
            overrideToneMap = needValue("--tonemap");
        } else if (a == "--white") {
            overrideWhite = std::atof(needValue("--white").c_str());
        } else if (a == "--aperture" || a == "--lensradius") {
            overrideAperture = std::atof(needValue("--aperture").c_str());
        } else if (a == "--focusdistance" || a == "--focus") {
            overrideFocus = std::atof(needValue("--focusdistance").c_str());
        } else if (a == "--fov") {
            overrideFov = std::atof(needValue("--fov").c_str());
        } else if (a == "--no-bvh") {
            forceNoBvh = true;
        } else if (a == "-q" || a == "--quiet") {
            quiet = true;
        } else if (!a.empty() && a[0] == '-') {
            std::cerr << "error: unknown option '" << a << "'\n";
            PrintUsage(argv[0]);
            return 2;
        } else if (scenePath.empty()) {
            scenePath = a;
        } else {
            std::cerr << "error: more than one scene file given ('" << a << "')\n";
            return 2;
        }
    }
    if (scenePath.empty()) {
        std::cerr << "error: no scene file given\n";
        PrintUsage(argv[0]);
        return 2;
    }
    if (outStem.empty()) outStem = StemOf(scenePath);

    Logger::Instance().SetEcho(!quiet);
    Logger::Instance().Open(outDir + "/" + outStem + ".log");
    LOGI("CGR26_renderer starting");
    LOGI("command line: scene='" + scenePath + "' output='" + outDir + "'");

    ScopeTimer total("total");
    ScopeTimer parse("parse");

    ParseResult pr = ParseSceneFile(scenePath);
    Scene& scene = pr.scene;

    // Command-line overrides win over the scene file.
    if (overrideW > 0) scene.film.xResolution = overrideW;
    if (overrideH > 0) scene.film.yResolution = overrideH;
    if (overrideSpp > 0) scene.spp = overrideSpp;
    if (overrideDepth > 0) scene.maxDepth = overrideDepth;
    if (overrideBF > 0) scene.branchingFactor = overrideBF;
    if (!overrideSampler.empty()) {
        if (overrideSampler == "uniform") scene.sampler = SamplerType::Uniform;
        else if (overrideSampler == "grid") scene.sampler = SamplerType::Grid;
        else if (overrideSampler == "halton") scene.sampler = SamplerType::Halton;
        else if (overrideSampler == "haltonjittered")
            scene.sampler = SamplerType::HaltonJittered;
        else
            LOGW("unknown sampler '" + overrideSampler + "' on the command line; ignored");
    }
    if (overrideWhite > 0) scene.whitePoint = overrideWhite;
    // Camera overrides are applied to the parameters the parser recorded, then
    // the camera is rebuilt from its own stored setup.
    {
        bool camChanged = false;
        if (overrideFov > 0) {
            scene.camera.setupFov = Clamp(overrideFov, 1e-3, 179.0);
            camChanged = true;
        }
        if (overrideAperture >= 0) {
            scene.camera.setupAperture = overrideAperture;
            camChanged = true;
        }
        if (overrideFocus > 0) {
            scene.camera.setupFocus = overrideFocus;
            camChanged = true;
        }
        // An aperture override also has to move the camera between the
        // pinhole and the thin-lens configuration.
        if (overrideAperture >= 0 && scene.camera.lastSetup == Camera::Setup::Perspective)
            scene.camera.lastSetup = Camera::Setup::ThinLens;
        if (overrideAperture >= 0 && overrideAperture == 0 &&
            scene.camera.lastSetup == Camera::Setup::ThinLens)
            scene.camera.lastSetup = Camera::Setup::Perspective;
        if (camChanged) {
            scene.camera.Reconfigure();
            LOGI("camera override: " + scene.camera.ProjectionName() +
                 ", fov=" + std::to_string(scene.camera.fov) + " aperture=" +
                 std::to_string(scene.camera.lensRadius) + " focus=" +
                 std::to_string(scene.camera.focusDistance));
        }
    }
    if (!overrideToneMap.empty()) {
        if (overrideToneMap == "reinhardluminance" || overrideToneMap == "luminance")
            scene.perChannelToneMap = false;
        else if (overrideToneMap == "reinhard" || overrideToneMap == "perchannel")
            scene.perChannelToneMap = true;
        else
            LOGW("unknown tone mapping '" + overrideToneMap + "' on the command line; ignored");
    }

    LOGI("parsed scene: " + std::to_string(scene.shapes.size()) + " shapes, " +
         std::to_string(scene.lights.size()) + " lights, " +
         std::to_string(scene.materials.size()) + " materials, integrator='" +
         scene.integrator + "'");
    if (scene.shapes.empty())
        LOGW("the scene contains no shapes; the image will show only the background");

    parse.Stop();

    // ---- acceleration ----------------------------------------------------
    BVH bvh;
    bool useBvh = !forceNoBvh && BVH::Enabled();
    if (useBvh) {
        bvh.Build(scene.shapes);
    } else {
        LOGI("acceleration disabled: brute-force intersection over " +
             std::to_string(scene.shapes.size()) + " primitives");
    }

    // ---- render ----------------------------------------------------------
    auto integrator = MakeIntegrator(scene.integrator);
    LOGI("integrator: " + integrator->Name());
    Image film = integrator->Render(scene, useBvh ? &bvh : nullptr);

    // ---- write -----------------------------------------------------------
    const char* ext = (format == ImageFormat::PNG) ? "png" : "ppm";
    std::string imgPath = outDir + "/" + outStem + "." + ext;
    bool ok = WriteImage(film, imgPath, format, scene.gammaCorrect);
    if (ok)
        LOGI("wrote " + imgPath + " (" + std::to_string(film.xSize) + "x" +
             std::to_string(film.ySize) + ")");
    else
        LOGE("failed to write " + imgPath);

    // Image statistics, useful for the test-suite comparisons.
    double sum[3] = {0, 0, 0};
    double mn = kInf, mx = 0;
    double sumSq[3] = {0, 0, 0};
    for (int y = 0; y < film.ySize; ++y)
        for (int x = 0; x < film.xSize; ++x) {
            Color c = film.GetPixel(x, y);
            for (int i = 0; i < 3; ++i) {
                sum[i] += c[i];
                sumSq[i] += c[i] * c[i];
                mn = std::min(mn, c[i]);
                mx = std::max(mx, c[i]);
            }
        }
    double n = static_cast<double>(film.xSize) * film.ySize;
    // Per-channel standard deviation: a cheap, resolution-independent measure of
    // how much detail the frame carries (used by the student tests and by the
    // Module 2 comparisons).
    double sd[3];
    for (int i = 0; i < 3; ++i) {
        double m = sum[i] / n;
        double v = sumSq[i] / n - m * m;
        sd[i] = v > 0 ? std::sqrt(v) : 0.0;
    }
    LOGI("image mean=(" + std::to_string(sum[0] / n) + ", " + std::to_string(sum[1] / n) +
         ", " + std::to_string(sum[2] / n) + ") min=" + std::to_string(mn) +
         " max=" + std::to_string(mx) + " stddev=(" + std::to_string(sd[0]) + ", " +
         std::to_string(sd[1]) + ", " + std::to_string(sd[2]) + ")");

    if (useBvh)
        LOGI("BVH stats: nodes=" + std::to_string(bvh.NumNodes()) + " leaves=" +
             std::to_string(bvh.NumLeaves()) + " rays=" + std::to_string(bvh.RayCount()) +
             " primTests=" + std::to_string(bvh.PrimTestCount()));
    // The acceleration structure keeps its own counters; fold them into the
    // profile so the timing report lists the same numbers.
    if (useBvh) {
        Profiler::Instance().AddRayCount(bvh.RayCount());
        Profiler::Instance().AddTriCount(bvh.PrimTestCount());
    }
    total.Stop();
    Profiler::Instance().Report();

    LOGI("parse warnings=" + std::to_string(pr.nWarnings) +
         " parse errors=" + std::to_string(pr.nErrors));
    LOGI("CGR26_renderer finished");
    Logger::Instance().Close();
    return ok ? 0 : 1;
}