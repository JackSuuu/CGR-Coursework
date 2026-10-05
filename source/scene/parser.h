#pragma once
// pbrt scene-file reader.
//
// The grammar handled here is the pbrt-v3/v4 subset the spec asks for:
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
    bool ok = false;      // false only if the file itself was unreadable
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
