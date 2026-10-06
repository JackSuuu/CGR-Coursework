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

    LOGI("parsed scene: " + std::to_string(scene.shapes.size()) + " shapes, " +
         std::to_string(scene.lights.size()) + " lights, " +
         std::to_string(scene.materials.size()) + " materials, integrator='" +
         scene.integrator + "'");
    if (scene.shapes.empty())
        LOGW("the scene contains no shapes; the image will show only the background");

    // ---- render ----------------------------------------------------------
    auto integrator = MakeIntegrator(scene.integrator);
    LOGI("integrator: " + integrator->Name());
    Image film = integrator->Render(scene);

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
    // how much detail the frame carries, used for the quantitative comparisons
    // in the report.
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

    Profiler::Instance().Report();

    LOGI("parse warnings=" + std::to_string(pr.nWarnings) +
         " parse errors=" + std::to_string(pr.nErrors));
    LOGI("CGR26_renderer finished");
    Logger::Instance().Close();
    return ok ? 0 : 1;
}
