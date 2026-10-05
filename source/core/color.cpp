#include "core/color.h"

#include <cctype>
#include <cstdlib>
#include <map>

namespace cgr {

namespace {
struct NamedColor {
    const char* name;
    double r, g, b;
};
// A small subset of pbrt-v4's named colours, plus a few convenient extras.
const NamedColor kNamed[] = {
    {"black", 0, 0, 0},
    {"white", 1, 1, 1},
    {"red", 1, 0, 0},
    {"green", 0, 1, 0},
    {"blue", 0, 0, 1},
    {"yellow", 1, 1, 0},
    {"cyan", 0, 1, 1},
    {"magenta", 1, 0, 1},
    {"gray", 0.5, 0.5, 0.5},
    {"grey", 0.5, 0.5, 0.5},
    {"silver", 0.75, 0.75, 0.75},
    {"orange", 1.0, 0.5, 0.0},
    {"pink", 1.0, 0.5, 0.5},
    {"brown", 0.5, 0.25, 0.0},
    {"purple", 0.5, 0.0, 0.5},
    {"gold", 1.0, 0.84, 0.0},
};
}  // namespace

const char* ColorTypeName() { return "color"; }

Color ParseColorSpec(const std::string& spec, bool* ok) {
    if (ok) *ok = true;
    std::string s = spec;
    // Strip a trailing component-count type token such as "rgb" or "xyz".
    auto lastSpace = s.find_last_of(' ');
    std::string tail = (lastSpace == std::string::npos) ? s : s.substr(lastSpace + 1);
    if (tail == "rgb" || tail == "xyz" || tail == "srgb") s = s.substr(0, lastSpace);

    // Named colour?
    std::string lower = s;
    for (auto& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    for (const auto& nc : kNamed) {
        if (lower == nc.name) return Color(nc.r, nc.g, nc.b);
    }

    // Numeric triple.
    double v[3] = {0, 0, 0};
    const char* p = s.c_str();
    char* end = nullptr;
    for (int i = 0; i < 3; ++i) {
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0') {
            if (ok) *ok = false;
            return Color(0);
        }
        v[i] = std::strtod(p, &end);
        if (end == p) {
            if (ok) *ok = false;
            return Color(0);
        }
        p = end;
    }
    return Color(v[0], v[1], v[2]);
}

}  // namespace cgr
