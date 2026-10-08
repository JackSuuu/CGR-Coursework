#pragma once
// pbrt scene-file reader.
//
// Supported standard PBRT subset: Film, Sampler, Integrator, LookAt, Camera,
// WorldBegin/End, Material/NamedMaterial, PPM imagemap Texture, LightSource
// (point), AreaLightSource (diffuse), Shape (sphere/inline trianglemesh),
// Translate/Scale/Rotate, Identity and AttributeBegin/End.
// The original named block syntax is also accepted:
//
//   <directive> [ "name" ] { <tokens> }      "string" { ... }
//   Shape "name" { "type" ... [ <transform> ] "material" [ ... ] }
//   ...
//
// Errors are never fatal: a malformed line is logged with its line number and
// the reader resynchronises, so a scene with deliberate mistakes still produces
// a .log the marker can inspect (Module 3's error-handling tests depend on it).
#include <string>
#include <vector>

#include "scene/scene.h"

namespace cgr {

struct ParseResult {
    bool ok = false;      // false if the file is unreadable or parsing has errors
    Scene scene;
    int nWarnings = 0;
    int nErrors = 0;
    int nLines = 0;
};

ParseResult ParseSceneFile(const std::string& path);

// Exposed for the unit tests / error-handling cases.
struct Token {
    std::string text;
    int line = 0;
    int col = 0;
};
std::vector<Token> Tokenize(const std::string& text);

}  // namespace cgr
