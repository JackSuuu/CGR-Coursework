#include "scene/parser.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "core/timer.h"
#include "scene/mesh.h"

namespace cgr {

MaterialPtr DefaultMaterial() {
    static MaterialPtr m = MakeMaterial(MaterialType::Diffuse);
    return m;
}

//
// Tokenizer
//
// The pbrt grammar is line-oriented and mixes bare numbers with quoted strings,
// so the reader works on a token stream where "\n" and '"' prefixes are
// significant. Unknown characters become their own token so the parser can
// report them instead of skipping them silently.
//
namespace {

bool IsIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}
bool IsIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' ||
           c == '.' || c == '/' || c == ':';
}
bool IsQuoted(const std::string& t) { return !t.empty() && t[0] == '"'; }
std::string Unquote(const std::string& t) { return IsQuoted(t) ? t.substr(1) : t; }
std::string ToLower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool ToDouble(const std::string& s, double* v) {
    if (s.empty()) return false;
    char* end = nullptr;
    double d = std::strtod(s.c_str(), &end);
    if (end == s.c_str()) return false;
    while (end && *end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (end && *end != '\0') return false;  // "12abc" is malformed
    *v = d;
    return true;
}

bool IsTransformOp(const std::string& t) {
    static const char* kOps[] = {"translate", "scale", "rotate", "matrix",
                                 "concattransform", "reversetransform",
                                 "concatreverse"};
    std::string l = ToLower(Unquote(t));
    for (const char* o : kOps)
        if (l == o) return true;
    return false;
}

std::vector<double> NumbersFromTokens(const std::vector<std::string>& toks) {
    std::vector<double> out;
    for (const auto& t : toks) {
        if (t == ",") continue;
        double v;
        if (ToDouble(t, &v)) out.push_back(v);
    }
    return out;
}

// Object-to-world transform for a rectangle emitter. AreaLight samples its
// local frame as x,y in [-1,1] at z = 0 emitting towards +z, so the two edge
// vectors have to be scaled by half and the third row has to be the unit
// normal (keeping the matrix non-singular so GetInverse() works).
Transform RectTransform(Point3 p00, Point3 p10, Point3 p01) {
    Vec3 ex = p10 - p00;
    Vec3 ey = p01 - p00;
    Vec3 exd = ex * 0.5;  // local x in [-1,1] spans the full edge
    Vec3 eyd = ey * 0.5;
    Vec3 n = Cross(exd, eyd);
    double nl = Length(n);
    Vec3 nd = nl > 0 ? n / nl : Vec3(0, 0, 1);
    // p10 = p00 + ex and p01 = p00 + ey by construction, so the centre of the
    // unit square's image is the midpoint of those two edges' far corners.
    Point3 c = p00 + (ex + ey) * 0.5;
    double m[4][4] = {{exd.x, exd.y, exd.z, c.x},
                      {eyd.x, eyd.y, eyd.z, c.y},
                      {nd.x, nd.y, nd.z, c.z},
                      {0, 0, 0, 1}};
    return Transform::FromMatrix(m);
}

// True for the pbrt parameter-type tokens, which may precede a parameter name.
bool IsTypeToken(const std::string& t) {
    static const char* kTypes[] = {"float",   "integer", "string",  "bool",
                                   "color",   "point3",  "vector3", "normal",
                                   "point",   "vector",  "blackbody", "spectrum",
                                   "rgb",     "xyz",     "srgb",    "triangle"};
    for (const char* s : kTypes)
        if (ToLower(t) == s) return true;
    return false;
}

}  // namespace

std::vector<Token> Tokenize(const std::string& text) {
    std::vector<Token> out;
    size_t i = 0;
    int line = 1, col = 1;
    auto adv = [&]() {
        if (i >= text.size()) return;
        if (text[i] == '\n') {
            ++line;
            col = 1;
        } else {
            ++col;
        }
        ++i;
    };
    while (i < text.size()) {
        char c = text[i];
        if (c == '#') {
            while (i < text.size() && text[i] != '\n') adv();
            continue;
        }
        if (c == '\n') {
            out.push_back(Token{"\n", line, col});
            adv();
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) {
            adv();
            continue;
        }
        if (c == '"') {
            int ln = line, cl = col;
            adv();
            std::string val;
            bool closed = false;
            while (i < text.size()) {
                if (text[i] == '"') {
                    adv();
                    closed = true;
                    break;
                }
                if (text[i] == '\\' && i + 1 < text.size()) {
                    adv();
                    char e = text[i];
                    val.push_back(e == 'n' ? '\n' : (e == 't' ? '\t' : e));
                    adv();
                    continue;
                }
                val.push_back(text[i]);
                adv();
            }
            out.push_back(Token{"\"" + val, ln, cl});
            if (!closed) {
                out.push_back(Token{"UNTERMINATED_STRING", ln, cl});
                break;
            }
            continue;
        }
        if (c == '[' || c == ']' || c == '(' || c == ')' || c == '{' || c == '}' ||
            c == ',') {
            out.push_back(Token{std::string(1, c), line, col});
            adv();
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' ||
            c == '.') {
            int ln = line, cl = col;
            std::string num;
            if (c == '-' || c == '+') {
                num.push_back(c);
                adv();
            }
            while (i < text.size()) {
                char d = text[i];
                if (std::isdigit(static_cast<unsigned char>(d)) || d == '.') {
                    num.push_back(d);
                    adv();
                } else if ((d == 'e' || d == 'E') && i + 1 < text.size() &&
                           (text[i + 1] == '-' || text[i + 1] == '+' ||
                            std::isdigit(static_cast<unsigned char>(text[i + 1])))) {
                    num.push_back(d);
                    adv();
                    num.push_back(text[i]);
                    adv();
                } else {
                    break;
                }
            }
            out.push_back(Token{num, ln, cl});
            continue;
        }
        if (IsIdentStart(c)) {
            int ln = line, cl = col;
            std::string id;
            while (i < text.size() && IsIdentChar(text[i])) {
                id.push_back(text[i]);
                adv();
            }
            out.push_back(Token{id, ln, cl});
            continue;
        }
        out.push_back(Token{std::string("?") + c, line, col});
        adv();
    }
    out.push_back(Token{"EOF", line, col});
    return out;
}

namespace {

// A parsed "key value(s)" entry from an option/material/shape block.
struct Entry {
    std::string type;    // pbrt type token if one was present
    std::string key;     // parameter name
    std::string value;   // first value token (unquoted)
    std::vector<std::string> values;  // all value tokens
    int line = 0;
};

class SceneReader {
  public:
    SceneReader(std::vector<Token> toks, std::string baseDir)
        : baseDir_(std::move(baseDir)), t_(std::move(toks)) {}

    bool Parse(Scene* scene);

    int nWarn = 0, nError = 0;
    std::string baseDir_;

  private:
    // ---- token stream helpers ----
    const Token& Cur() const { return t_[p_]; }
    void Next() {
        if (p_ + 1 < t_.size()) ++p_;
    }
    bool Eof() const { return Cur().text == "EOF"; }
    void SkipSeparators() {
        while (Cur().text == "\n" || Cur().text == "," || Cur().text == " ") Next();
    }
    void Error(int line, const std::string& msg) {
        ++nError;
        Logger::Instance().Problem("line " + std::to_string(line), msg);
    }
    void Error(const Token& tok, const std::string& msg) { Error(tok.line, msg); }
    void Warn(int line, const std::string& msg) {
        ++nWarn;
        Logger::Instance().Write(LogLevel::Warn, "line " + std::to_string(line) + ": " + msg);
    }
    void Warn(const Token& tok, const std::string& msg) { Warn(tok.line, msg); }

    std::string ResolvePath(const std::string& rel) const {
        if (rel.empty() || rel[0] == '/') return rel;
        return baseDir_ + "/" + rel;
    }

    // Reads a bracketed/parenthesised group, returning its inner tokens.
    std::vector<std::string> ReadGroup();
    // Reads a value: either a bracket group or a run of bare tokens up to the
    // next quoted string / closing brace.
    std::vector<std::string> ReadValue(int arity = 1);
    // Parses the rest of the current block into key/value entries.
    std::vector<Entry> ReadEntries();
    // Consumes up to and including the matching '}'.
    void SkipBlock();
    Transform ReadTransform();
    // One bare transform operation, e.g. "translate" -1 0 2.
    Transform ReadTransformOp();
    const Token& PeekAhead(int n) const {
        size_t q = p_ + n;
        return t_[q < t_.size() ? q : t_.size() - 1];
    }

    void ParseCamera(Scene* scene);
    void ParseIntegrator(const std::string& name, Scene* scene);
    void ParseFilm(Scene* scene);
    void ParseShape(const std::string& name, Scene* scene);
    void ParseLight(const std::string& dir, const std::string& type, Scene* scene);
    void ParseMaterial(const std::string& name, Scene* scene);
    void ParseTriangleMesh(const std::string& name, const std::vector<Entry>& entries,
                           const Transform& xf, Scene* scene);
    Material* ResolveMaterial(Scene* scene, const std::string& name);
    void ApplyTexture(Material* m, const std::vector<Entry>& entries);
    TexturePtr LoadTextureFrom(const std::vector<Entry>& entries);

    std::vector<Token> t_;
    size_t p_ = 0;
    std::map<std::string, MaterialPtr> namedMaterials_;
    std::map<std::string, std::vector<ShapePtr>> namedShapes_;
};

std::vector<std::string> SceneReader::ReadGroup() {
    std::vector<std::string> out;
    if (Cur().text != "[" && Cur().text != "(") {
        Error(Cur(), "expected '[' or '(' but found '" + Cur().text + "'");
        return out;
    }
    Next();  // opening bracket
    int depth = 1;
    while (!Eof() && depth > 0) {
        const std::string& s = Cur().text;
        if (s == "[" || s == "(") {
            ++depth;
        } else if (s == "]" || s == ")") {
            --depth;
            if (depth == 0) {
                Next();
                break;
            }
        } else {
            out.push_back(s);
        }
        Next();
    }
    if (depth > 0) Error(Cur(), "unterminated list: expected a matching ']' or ')'");
    return out;
}

// A handful of pbrt parameters legitimately take several quoted values, e.g.
//   "texture" "diffuse" "checker.ppm"
// The slot name and the file are indistinguishable from a key/value pair by
// local inspection, so the arity is looked up for those keys.
int QuotedArity(const std::string& key) {
    if (key == "texture" || key == "coefficients" || key == "map") return 2;
    return 1;
}

// A parameter value is either a bracket group or a run of bare tokens that ends
// at the next quoted string, a newline or a closing brace.
std::vector<std::string> SceneReader::ReadValue(int arity) {
    if (Cur().text == "[" || Cur().text == "(") return ReadGroup();
    std::vector<std::string> out;
    while (!Eof()) {
        const std::string& s = Cur().text;
        if (s == "\n" || s == "}" || s == "{" || s == "," || s == "EOF") break;
        if (IsQuoted(s)) {
            // A quoted token after the first value is the next parameter name --
            // unless this parameter is known to take several strings, in which
            // case it is the following value of the current parameter.
            if (!out.empty() && arity <= 1) break;
            out.push_back(s);
            Next();
            if (static_cast<int>(out.size()) >= arity) break;
            continue;
        }
        out.push_back(s);
        Next();
    }
    return out;
}

void SceneReader::SkipBlock() {
    int depth = 0;
    while (!Eof()) {
        const std::string& s = Cur().text;
        if (s == "{") ++depth;
        if (s == "}") {
            --depth;
            Next();
            if (depth <= 0) return;
            continue;
        }
        if (s == "\n" && depth == 0) return;
        Next();
    }
}

std::vector<Entry> SceneReader::ReadEntries() {
    std::vector<Entry> entries;
    std::string pendingType;
    for (;;) {
        SkipSeparators();
        if (Eof() || Cur().text == "}") break;
        if (Cur().text == "{") {  // a nested block we do not interpret
            SkipBlock();
            continue;
        }
        Token keyTok = Cur();
        std::string raw = Unquote(Cur().text);
        Next();
        // pbrt-v4 folds the type and the name into a single quoted token,
        // e.g. "integer maxdepth" or "float fov". Split on whitespace and let
        // a leading type word set the pending type.
        std::istringstream keyStream(raw);
        std::vector<std::string> words;
        std::string w;
        while (keyStream >> w) words.push_back(w);
        if (words.size() > 1 && IsTypeToken(words[0])) pendingType = ToLower(words[0]);
        std::string key = words.empty() ? raw : words.back();

        // A bare, valueless word such as "diffuse" usually names the type of the
        // enclosing block. Shape and light names ("point", "sphere", ...) are
        // part of the same token list, so those are recognised by the bracket
        // group that follows them and are treated as a normal parameter instead.
        if (words.size() == 1 && IsTypeToken(words[0]) && Cur().text != "[" &&
            Cur().text != "(") {
            pendingType = ToLower(words[0]);
            continue;
        }
        Entry e;
        e.type = pendingType;
        pendingType.clear();
        e.key = ToLower(key);
        e.line = keyTok.line;
        e.values = ReadValue(QuotedArity(e.key));
        if (!e.values.empty()) e.value = Unquote(e.values.front());
        entries.push_back(e);
    }
    if (!Eof() && Cur().text == "}") Next();
    return entries;
}

Transform SceneReader::ReadTransform() {
    Transform xf;
    bool bracketed = (Cur().text == "[" || Cur().text == "(");
    if (bracketed) Next();
    while (!Eof()) {
        SkipSeparators();
        if (bracketed && (Cur().text == "]" || Cur().text == ")")) {
            Next();
            break;
        }
        if (Cur().text == "}") break;
        if (!bracketed && IsQuoted(Cur().text)) break;  // next parameter name
        if (!IsTransformOp(Cur().text)) {
            Warn(Cur(), "expected a transform operation but found '" + Cur().text + "'");
            break;
        }
        xf = xf * ReadTransformOp();
        SkipSeparators();
        if (!bracketed && IsQuoted(Cur().text)) break;
    }
    return xf;
}

// Reads one transform operation, e.g. "translate" 1 2 3 or "rotate" 1 0 0 45.
// Shared by the bracketed and the bare spellings.
Transform SceneReader::ReadTransformOp() {
    Transform xf;
    Token opTok = Cur();
    std::string op = ToLower(Unquote(Cur().text));
    Next();
        // Reads up to `n` numbers, stopping at any non-numeric token.
        auto nums = [&](size_t n, std::vector<double>* out) {
            out->clear();
            while (!Eof() && out->size() < n) {
                SkipSeparators();
                if (Cur().text == "]" || Cur().text == ")" || Cur().text == "}" ||
                    Cur().text == "\n")
                    break;
                if (Cur().text == ",") {
                    Next();
                    continue;
                }
                double v;
                if (!ToDouble(Cur().text, &v)) break;
                out->push_back(v);
                Next();
            }
            return out->size() == n;
        };
        std::vector<double> v;
        if (op == "translate" || op == "scale") {
            if (!nums(3, &v)) nums(1, &v);
            if (v.size() == 3) {
                xf = xf * (op == "translate" ? Transform::Translate(Vec3(v[0], v[1], v[2]))
                                            : Transform::Scale(v[0], v[1], v[2]));
            } else if (v.size() == 1) {
                xf = xf * (op == "translate" ? Transform::Translate(Vec3(v[0], v[0], v[0]))
                                            : Transform::Scale(v[0]));
            } else {
                Error(opTok, "'" + op + "' needs 1 or 3 numbers");
            }
        } else if (op == "rotate") {
            if (nums(3, &v)) {
                // pbrt accepts both "rotate <rx ry rz> <deg>" (a scaled axis) and
                // "rotate 1 0 0 <deg>" (a unit axis selector). Distinguish them
                // by whether the next token is another number.
                double ang = 0;
                SkipSeparators();
                if (ToDouble(Cur().text, &ang)) {
                    Next();
                    if ((v[0] == 1 && v[1] == 0 && v[2] == 0) ||
                        (v[0] == 0 && v[1] == 1 && v[2] == 0) ||
                        (v[0] == 0 && v[1] == 0 && v[2] == 1)) {
                        if (v[0] == 1) xf = xf * Transform::RotateX(ang);
                        else if (v[1] == 1) xf = xf * Transform::RotateY(ang);
                        else xf = xf * Transform::RotateZ(ang);
                    } else {
                        xf = xf * Transform::Rotate(Vec3(v[0], v[1], v[2]), ang);
                    }
                } else {
                    Error(Cur(), "'rotate' needs an angle after the axis");
                }
            } else {
                Error(opTok, "'rotate' needs 3 numbers (rx ry rz)");
            }
        } else if (op == "matrix" || op == "concattransform") {
            if (nums(16, &v)) {
                double m[4][4];
                for (int i = 0; i < 4; ++i)
                    for (int j = 0; j < 4; ++j) m[i][j] = v[i * 4 + j];
                xf = xf * Transform::FromMatrix(m);
            } else {
                Error(opTok, "'" + op + "' needs 16 numbers");
            }
        } else if (op == "reversetransform" || op == "concatreverse") {
            // Apply the pending transform's inverse, matching pbrt's semantics
            // of "reverseorientation" on a transform stack.
            xf = Transform();
            LOGW("'" + op + "' is not supported; the transform was reset to identity");
        } else {
            Warn(opTok, "unsupported transform operation '" + op + "' ignored");
            while (!Eof() && Cur().text != "\n" && Cur().text != "]" &&
                   Cur().text != ")" && Cur().text != "}")
                Next();
        }
    return xf;
}

//
// Parameter accessors over a vector of Entry.
//
namespace {

const Entry* FindEntry(const std::vector<Entry>& es, const char* key) {
    for (const auto& e : es)
        if (e.key == key) return &e;
    return nullptr;
}

bool EntryNumbers(const Entry& e, std::vector<double>* out) {
    out->clear();
    for (const auto& tok : e.values) {
        if (tok == ",") continue;
        if (IsQuoted(tok)) {
            std::istringstream is(Unquote(tok));
            std::string s;
            while (is >> s) {
                double v;
                if (ToDouble(s, &v)) out->push_back(v);
            }
            continue;
        }
        double v;
        if (!ToDouble(tok, &v)) return false;
        out->push_back(v);
    }
    return !out->empty();
}

bool GetNum1(const std::vector<Entry>& es, const char* key, double* v) {
    const Entry* e = FindEntry(es, key);
    if (!e) return false;
    std::vector<double> n;
    if (!EntryNumbers(*e, &n) || n.empty()) return false;
    *v = n[0];
    return true;
}

bool GetInt1(const std::vector<Entry>& es, const char* key, int* v) {
    double d;
    if (!GetNum1(es, key, &d)) return false;
    *v = static_cast<int>(d);
    return true;
}

bool GetCol(const std::vector<Entry>& es, const char* key, Color* c) {
    const Entry* e = FindEntry(es, key);
    if (!e) return false;
    if (e->values.size() == 1 && IsQuoted(e->values[0])) {
        bool ok = true;
        *c = ParseColorSpec(Unquote(e->values[0]), &ok);
        return ok;
    }
    std::vector<double> n;
    if (!EntryNumbers(*e, &n) || n.size() < 3) return false;
    *c = Color(n[0], n[1], n[2]);
    return true;
}

std::string EntryString(const std::vector<Entry>& es, const char* key) {
    const Entry* e = FindEntry(es, key);
    if (!e || e->values.empty()) return "";
    return Unquote(e->values.front());
}

}  // namespace

Material* SceneReader::ResolveMaterial(Scene* scene, const std::string& name) {
    if (name.empty()) {
        if (!scene->materials.empty()) return scene->materials.front().get();
        return DefaultMaterial().get();
    }
    auto it = namedMaterials_.find(name);
    if (it != namedMaterials_.end()) return it->second.get();
    Logger::Instance().Warn("material '" + name +
                            "' is referenced but never declared; using a default "
                            "diffuse material");
    auto m = MakeMaterial(MaterialType::Diffuse);
    namedMaterials_[name] = m;
    scene->materials.push_back(m);
    return m.get();
}

// Locates and loads a PPM texture named by one of the texture-ish parameters.
TexturePtr SceneReader::LoadTextureFrom(const std::vector<Entry>& entries) {
    static const char* kKeys[] = {"diffusemap", "texture", "map", "bumpmap",
                                  "emissionmap"};
    std::string file;
    int line = 0;
    for (const char* k : kKeys) {
        const Entry* e = FindEntry(entries, k);
        if (!e) continue;
        if (e->values.size() >= 2)
            file = Unquote(e->values.back());   // "texture" "diffuse" "f.ppm"
        else if (e->values.size() == 1)
            file = Unquote(e->values.front());
        if (!file.empty()) {
            line = e->line;
            break;
        }
    }
    if (file.empty()) return nullptr;
    std::string err;
    auto tex = LoadTexturePPM(ResolvePath(file), &err);
    if (!tex) {
        Logger::Instance().Problem("line " + std::to_string(line) + " texture '" + file +
                                   "'", err + "; falling back to the flat colour");
        ++nError;
        return nullptr;
    }
    double s;
    if (GetNum1(entries, "uscale", &s)) tex->uScale = s;
    if (GetNum1(entries, "vscale", &s)) tex->vScale = s;
    std::string w = ToLower(EntryString(entries, "wrap"));
    if (!w.empty()) {
        if (w == "clamp")
            tex->wrapS = tex->wrapT = TextureWrap::Clamp;
        else if (w == "black")
            tex->wrapS = tex->wrapT = TextureWrap::Black;
        else
            tex->wrapS = tex->wrapT = TextureWrap::Repeat;
    }
    Logger::Instance().Info("loaded texture '" + file + "' (" +
                            std::to_string(tex->width) + "x" +
                            std::to_string(tex->height) + ")");
    return tex;
}

void SceneReader::ApplyTexture(Material* m, const std::vector<Entry>& entries) {
    auto tex = LoadTextureFrom(entries);
    if (!tex) return;
    m->SetDiffuseTexture(tex);
    Color mod;
    if (GetCol(entries, "texmod", &mod)) m->textureModulate = mod;
}

void SceneReader::ParseMaterial(const std::string& name, Scene* scene) {
    std::vector<Entry> entries = ReadEntries();
    // pbrt-v3 puts the material type as a valueless first token:
    //   Material "m" { "diffuse" "reflectance" [ .8 .8 .8 ] }
    // pbrt-v3 puts the material type as the first token of the block, with no
    // value of its own:  Material "m" { "phong" "reflectance" [ ... ] }. The
    // tokenizer attaches the following parameter name to that first token, so
    // the type is recognised by the first key *not* being a known parameter.
    // That also lets an unknown type reach the warning below instead of being
    // silently treated as diffuse.
    static const char* kMaterialParams[] = {
        "reflectance",   "diffusecolor", "kd",         "basecolor",  "specularreflectance",
        "ks",            "specularcolor", "emission",  "ambient",    "shininess",
        "phongexponent", "exponent",     "eta",        "roughness",  "texmod",
        "texture",       "string",       "float",      "integer",    "bool",
        "rgb",           "spectrum",     "blackbody",  "backoff",    "intensity"};
    std::string type = "diffuse";
    if (!entries.empty()) {
        bool isParam = false;
        for (const char* k : kMaterialParams)
            if (entries[0].key == k) {
                isParam = true;
                break;
            }
        if (!isParam) {
            type = entries[0].key;
            entries.erase(entries.begin());
        }
    }

    MaterialType mt = MaterialType::Diffuse;
    if (type == "mirror")
        mt = MaterialType::Mirror;
    else if (type == "dielectric" || type == "glass")
        mt = MaterialType::Dielectric;
    else if (type == "phong" || type == "blinnphong" || type == "blinn")
        mt = MaterialType::Phong;
    else if (type == "plastic")
        mt = MaterialType::Plastic;
    else if (type != "diffuse" && type != "lambertian" && type != "constant")
        Logger::Instance().Warn("unknown material type '" + type +
                                "'; using a diffuse material");

    auto m = MakeMaterial(mt);
    Color c;
    if (GetCol(entries, "reflectance", &c)) m->reflectance = c;
    else if (GetCol(entries, "diffusecolor", &c)) m->reflectance = c;
    else if (GetCol(entries, "kd", &c)) m->reflectance = c;
    else if (GetCol(entries, "basecolor", &c)) m->reflectance = c;
    if (GetCol(entries, "specularreflectance", &c)) m->specular = c;
    else if (GetCol(entries, "ks", &c)) m->specular = c;
    else if (GetCol(entries, "specularcolor", &c)) m->specular = c;
    if (GetCol(entries, "emission", &c)) m->emission = c;
    if (GetCol(entries, "ambient", &c)) m->ambientScale = c;
    double s;
    if (GetNum1(entries, "shininess", &s)) m->shininess = s;
    if (GetNum1(entries, "phongexponent", &s)) m->phongExponent = s;
    if (GetNum1(entries, "exponent", &s)) m->phongExponent = s;
    if (GetNum1(entries, "eta", &s)) {
        if (s <= 0) {
            Logger::Instance().Warn("material 'eta' must be > 0; clamping to 1.5");
            s = 1.5;
        }
        m->eta = s;
    }
    if (GetNum1(entries, "roughness", &s)) {
        double r = Clamp(s, 1e-3, 1.0);
        m->shininess = Clamp(2.0 / (r * r) - 2.0, 1.0, 1e6);
        m->phongExponent = m->shininess;
    }
    if (GetCol(entries, "texmod", &c)) m->textureModulate = c;
    ApplyTexture(m.get(), entries);
    namedMaterials_[name] = m;
    scene->materials.push_back(m);
}

void SceneReader::ParseTriangleMesh(const std::string& name,
                                    const std::vector<Entry>& entries,
                                    const Transform& xf, Scene* scene) {
    // "trianglemesh" "file.obj"  |  "trianglemesh" [ "obj" "file.obj" ]
    std::string file = EntryString(entries, "trianglemesh");
    if (file.empty()) {
        // pbrt-v4: "shape" "trianglemesh" "obj" "file.obj" indices
        const Entry* e = FindEntry(entries, "shape");
        if (e) file = Unquote(e->values.back());
    }
    if (file.empty()) {
        Error(0, "trianglemesh '" + name + "' has no file");
        return;
    }
    std::string err;
    auto mesh = LoadOBJ(ResolvePath(file), &err);
    if (!mesh) {
        Logger::Instance().Problem("mesh '" + file + "'", err + "; mesh skipped");
        ++nError;
        return;
    }
    auto tris = mesh->BuildTriangles(xf);
    if (tris.size() + scene->shapes.size() > static_cast<size_t>(scene->maxPrimitives)) {
        Logger::Instance().Warn("mesh '" + file + "' skipped: the scene would exceed the " +
                                std::to_string(scene->maxPrimitives) + " primitive limit");
        return;
    }
    Material* m = ResolveMaterial(scene, EntryString(entries, "material"));
    bool reverse = false;
    std::string ro = ToLower(EntryString(entries, "reverseorientation"));
    if (ro == "true" || ro == "1") reverse = true;
    Color emis;
    bool hasEmis = GetCol(entries, "emission", &emis);
    for (auto& t : tris) {
        if (reverse) {
            std::swap(t->p[1], t->p[2]);
            std::swap(t->uv[1], t->uv[2]);
            std::swap(t->sn[1], t->sn[2]);
            t->e1 = t->p[1] - t->p[0];
            t->e2 = t->p[2] - t->p[0];
            t->n = Normalize(Cross(t->e1, t->e2));
        }
        t->material = m;
        t->isLightEmitter = hasEmis;
        t->emission = emis;
        scene->shapes.push_back(t);
        namedShapes_[name].push_back(t);
    }
    scene->numTriangles += static_cast<int>(tris.size());
    ++scene->numMeshFiles;
    Logger::Instance().Info("loaded mesh '" + file + "': " +
                            std::to_string(tris.size()) + " triangles");
}

void SceneReader::ParseShape(const std::string& name, Scene* scene) {
    SkipSeparators();
    Token typeTok = Cur();
    if (!IsQuoted(Cur().text)) {
        Error(typeTok, "expected a quoted shape type inside Shape \"" + name + "\"");
        SkipBlock();
        return;
    }
    std::string type = ToLower(Unquote(Cur().text));
    Next();
    if (type == "trianglemesh" || type == "plymesh" || type == "bilinearmesh") {
        // Mesh blocks carry the filename as the first value after the type.
        std::vector<std::string> vals = ReadValue();
        std::vector<Entry> rest = ReadEntries();
        std::vector<Entry> all;
        Entry fileEntry;
        fileEntry.key = "trianglemesh";
        fileEntry.values = vals;
        all.push_back(fileEntry);
        for (auto& e : rest) all.push_back(e);
        if (type != "trianglemesh")
            Logger::Instance().Warn("mesh shape '" + type + "' is read as a trianglemesh");
        ParseTriangleMesh(name, all, Transform(), scene);
        return;
    }

    // A bracketed group straight after the type is the primitive's own
    // parameter (sphere radius, triangle vertex positions) *unless* its first
    // token is a transform operation, in which case it is the object-to-world
    // transform. Both spellings occur in the wild, so the two cases are told
    // apart here.
    Transform xf;
    std::vector<double> posNums;
    SkipSeparators();
    if (Cur().text == "[" || Cur().text == "(") {
        if (IsTransformOp(PeekAhead(1).text)) {
            xf = ReadTransform();
        } else {
            posNums = NumbersFromTokens(ReadGroup());
        }
    }
    // pbrt-v3 also allows the positional numbers as bare tokens, and the
    // transform as bare "translate"/"scale"/"rotate" sequences.
    SkipSeparators();
    while (!Eof() && IsTransformOp(Cur().text)) xf = xf * ReadTransformOp();
    SkipSeparators();
    while (!Eof() && Cur().text != "\n" && Cur().text != "}" && Cur().text != "{" &&
           !IsQuoted(Cur().text)) {
        double v;
        if (!ToDouble(Cur().text, &v)) break;
        posNums.push_back(v);
        Next();
        SkipSeparators();
    }
    auto entries = ReadEntries();
    std::string matName = EntryString(entries, "material");
    Material* m = ResolveMaterial(scene, matName);
    Color emis;
    bool hasEmis = GetCol(entries, "emission", &emis);
    bool reverse = ToLower(EntryString(entries, "reverseorientation")) == "true";

    ShapePtr shape;
    if (type == "sphere") {
        const Entry* e = FindEntry(entries, "radius");
        double r = 1.0;
        if (e) {
            std::vector<double> v;
            if (EntryNumbers(*e, &v) && !v.empty()) r = v[0];
        } else if (!posNums.empty()) {
            r = posNums[0];
        }
        if (r <= 0) {
            Error(typeTok.line, "sphere radius must be positive (got " +
                                    std::to_string(r) + "); using |radius|");
            r = std::fabs(r);
            if (r == 0) r = 1.0;
        }
        shape = std::make_shared<Sphere>(r);
    } else if (type == "plane") {
        shape = std::make_shared<Plane>();
    } else if (type == "triangle") {
        auto tri = std::make_shared<Triangle>();
        std::vector<double> v = posNums;
        const Entry* e = FindEntry(entries, "p");
        if (!v.empty()) {
            if (v.size() < 9) {
                Error(typeTok.line, "'triangle' needs 9 numbers, got " +
                                        std::to_string(v.size()));
                v.resize(9, 0.0);
            }
            for (int i = 0; i < 3; ++i)
                tri->p[i] = Vec3(v[3 * i], v[3 * i + 1], v[3 * i + 2]);
            tri->e1 = tri->p[1] - tri->p[0];
            tri->e2 = tri->p[2] - tri->p[0];
            tri->n = Normalize(Cross(tri->e1, tri->e2));
        } else if (e) {
            std::vector<double> pv;
            if (EntryNumbers(*e, &pv) && pv.size() >= 9)
                for (int i = 0; i < 3; ++i)
                    tri->p[i] = Vec3(pv[3 * i], pv[3 * i + 1], pv[3 * i + 2]);
            tri->e1 = tri->p[1] - tri->p[0];
            tri->e2 = tri->p[2] - tri->p[0];
            tri->n = Normalize(Cross(tri->e1, tri->e2));
        }
        auto uvEntry = FindEntry(entries, "uv");
        if (uvEntry) {
            std::vector<double> uvv;
            if (EntryNumbers(*uvEntry, &uvv))
                for (int i = 0; i < 3 && 2 * i + 1 < static_cast<int>(uvv.size()); ++i)
                    tri->uv[i] = Point2(uvv[2 * i], uvv[2 * i + 1]);
        }
        shape = tri;
    } else if (type == "disk" || type == "cone" || type == "paraboloid" ||
               type == "hyperboloid" || type == "bilinearquad" || type == "bilinear" ||
               type == "curve" || type == "loopsubdiv" || type == "tesselate" ||
               type == "ply" || type == "trianglerope" || type == "hair") {
        Logger::Instance().Warn("shape type '" + type +
                                "' is not implemented; the shape was skipped");
        ++nWarn;
        return;
    } else {
        Error(typeTok.line, "unknown shape type '" + type + "'");
        return;
    }

    shape->SetTransform(xf);
    shape->material = m;
    shape->isLightEmitter = hasEmis;
    shape->emission = emis;
    if (reverse) {
        if (auto tri = std::dynamic_pointer_cast<Triangle>(shape)) {
            std::swap(tri->p[1], tri->p[2]);
            std::swap(tri->uv[1], tri->uv[2]);
            tri->e1 = tri->p[1] - tri->p[0];
            tri->e2 = tri->p[2] - tri->p[0];
            tri->n = Normalize(Cross(tri->e1, tri->e2));
        }
    }
    // Inline material parameters on the shape override the referenced material.
    bool inlineMat = false;
    for (const auto& e : entries) {
        if (e.key == "material" || e.key == "emission" ||
            e.key == "reverseorientation")
            continue;
        if (e.key == "radius" || e.key == "p" || e.key == "uv" ||
            e.key == "texture" || e.key == "uscale" || e.key == "vscale" ||
            e.key == "wrap" || e.key == "texmod" || e.key == "trianglemesh")
            continue;
        inlineMat = true;
    }
    if (inlineMat) {
        auto copy = MakeMaterial(m->IsMirror()     ? MaterialType::Mirror
                                 : m->IsSpecularDelta() ? MaterialType::Dielectric
                                 : m->Name() == "phong" ? MaterialType::Phong
                                                        : MaterialType::BlinnPhong);
        copy->reflectance = m->reflectance;
        copy->specular = m->specular;
        copy->ambientScale = m->ambientScale;
        copy->shininess = m->shininess;
        copy->phongExponent = m->phongExponent;
        copy->eta = m->eta;
        Color c;
        if (GetCol(entries, "reflectance", &c)) copy->reflectance = c;
        if (GetCol(entries, "diffusecolor", &c)) copy->reflectance = c;
        if (GetCol(entries, "specularreflectance", &c)) copy->specular = c;
        if (GetCol(entries, "emission", &c)) copy->emission = c;
        double s;
        if (GetNum1(entries, "shininess", &s)) copy->shininess = s;
        if (GetNum1(entries, "exponent", &s)) copy->phongExponent = copy->shininess = s;
        if (GetNum1(entries, "eta", &s) && s > 0) copy->eta = s;
        if (FindEntry(entries, "diffusemap") || FindEntry(entries, "texture"))
            ApplyTexture(copy.get(), entries);
        scene->materials.push_back(copy);
        m = copy.get();
        shape->material = m;
    }
    if (shape->isLightEmitter) m->emission = emis;
    if (scene->shapes.size() >= static_cast<size_t>(scene->maxPrimitives)) {
        Logger::Instance().Warn("shape '" + name +
                                "' skipped: the primitive limit was reached");
        return;
    }
    scene->shapes.push_back(shape);
    namedShapes_[name].push_back(shape);
}

void SceneReader::ParseLight(const std::string& dir, const std::string& typeIn,
                             Scene* scene) {
    std::string type = ToLower(typeIn);
    auto entries = ReadEntries();
    std::string typeName = type;
    // AreaLight "n" { "rectangle" [ ... ] } -- the type is the first value.
    if (dir == "arealight" || dir == "pointlight" || dir == "distantlight" ||
        dir == "light") {
        const Entry* e = FindEntry(entries, "rectangle");
        if (e)
            typeName = "rectangle";
        else if (FindEntry(entries, "disk"))
            typeName = "disk";
        else if (FindEntry(entries, "sphere"))
            typeName = "sphere";
        else if (FindEntry(entries, "point"))
            typeName = "point";
        else if (FindEntry(entries, "distant") || FindEntry(entries, "sun"))
            typeName = "distant";
    }
    // pbrt-v3 spelling: AreaLight [ 0 4 4 6 ]  "I" [ ... ]
    if (typeName == "light" || typeName == "arealight" || typeName == "pointlight" ||
        typeName == "distantlight") {
        Error(0, "light '" + typeIn +
                      "' has no recognised shape; expected one of \"point\", "
                      "\"distant\", \"rectangle\", \"disk\" or \"sphere\"");
        return;
    }

    Color intensity(1), emission(1), scale(1);
    GetCol(entries, "intensity", &intensity);
    // pbrt-v4 renamed the point-light parameter; accept both spellings.
    GetCol(entries, "radiantintensity", &intensity);
    GetCol(entries, "color", &intensity);
    GetCol(entries, "radiance", &emission);
    GetCol(entries, "L", &emission);
    GetCol(entries, "scale", &scale);

    if (typeName == "point") {
        auto l = std::make_shared<PointLight>();
        Vec3 pos(0, 0, 0);
        const Entry* e = FindEntry(entries, "point");
        if (e) {
            std::vector<double> v;
            if (EntryNumbers(*e, &v) && v.size() >= 3)
                pos = Vec3(v[0], v[1], v[2]);
            else
                Error(e->line, "point light needs 3 numbers");
        } else {
            Error(0, "point light has no position");
        }
        l->position = pos;
        l->intensity = intensity * scale;
        scene->lights.push_back(l);
        std::ostringstream os;
        os << "point light at (" << pos.x << ", " << pos.y << ", " << pos.z << ") I=("
           << l->intensity.r << ", " << l->intensity.g << ", " << l->intensity.b << ")";
        Logger::Instance().Info(os.str());
        return;
    }
    if (typeName == "distant" || typeName == "sun") {
        auto l = std::make_shared<DistantLight>();
        const Entry* e = FindEntry(entries, "distant");
        if (e) {
            std::vector<double> v;
            if (EntryNumbers(*e, &v) && v.size() >= 3)
                l->direction = Normalize(Vec3(v[0], v[1], v[2]));
        }
        l->intensity = intensity * scale;
        scene->lights.push_back(l);
        Logger::Instance().Info("distant light");
        return;
    }
    if (typeName == "rectangle" || typeName == "disk" || typeName == "sphere") {
        auto l = std::make_shared<AreaLight>();
        const Entry* e = FindEntry(entries, typeName.c_str());
        if (e) {
            std::vector<double> v;
            if (EntryNumbers(*e, &v)) {
                if (typeName == "rectangle") {
                    if (v.size() >= 12) {
                        // Four corners, pbrt order: p00 p10 p11 p01. The local
                        // frame is built from the two edge vectors, so any
                        // orientation in the scene file works.
                        Point3 p00(v[0], v[1], v[2]), p10(v[3], v[4], v[5]);
                        Point3 p01(v[9], v[10], v[11]);
                        l->SetTransform(RectTransform(p00, p10, p01));
                    } else if (v.size() >= 6) {
                        // Two corners. Only the segment they define is
                        // determined, so the panel is made square and is
                        // extended along whichever world axis is most
                        // perpendicular to the segment (y first, then z, then x).
                        Point3 p0(v[0], v[1], v[2]), p1(v[3], v[4], v[5]);
                        Vec3 seg = p1 - p0;
                        double len = Length(seg);
                        if (len <= 0) {
                            Error(e->line, "rectangle corners coincide");
                        } else {
                            Vec3 ex = seg / len;
                            Vec3 up(0, 1, 0);
                            if (std::fabs(Dot(Normalize(up), ex)) > 0.9) up = Vec3(0, 0, 1);
                            Vec3 ey = Normalize(Cross(up, ex));
                            l->SetTransform(RectTransform(p0, p0 + ex * len, p0 + ey * len));
                        }
                    } else {
                        Error(e->line,
                              "'rectangle' needs 12 numbers (four corners) or 6 (two "
                              "opposite corners)");
                    }
                } else {
                    if (!v.empty() && v[0] > 0) {
                        l->radius = v[0];
                    } else {
                        Error(e->line, "'" + typeName + "' needs a positive radius");
                    }
                }
            } else {
                Error(e->line, "'" + typeName + "' needs numbers");
            }
        } else {
            Error(0, "area light has no shape specification");
        }
        l->shape = (typeName == "disk")  ? AreaLight::Shape::Disk
                   : (typeName == "sphere") ? AreaLight::Shape::Sphere
                                            : AreaLight::Shape::Rectangle;
        l->emission = emission;
        l->scale = scale;
        if (auto tex = LoadTextureFrom(entries)) l->emissionTexture = tex;
        scene->lights.push_back(l);
        Logger::Instance().Info("area light (" + typeName + ") area=" +
                                std::to_string(l->Area()));
        return;
    }
    Error(0, "unknown light type '" + typeIn + "'");
}

void SceneReader::ParseCamera(Scene* scene) {
    auto entries = ReadEntries();
    // pbrt-v3 form: "lookat" 0 1 5  0 1 0   (bare tokens, not an entry)
    Vec3 pos(0, 0, 1), look(0, 0, 0), up(0, 1, 0);
    bool haveLook = false;
    if (const Entry* e = FindEntry(entries, "lookat")) {
        std::vector<double> v;
        if (EntryNumbers(*e, &v) && v.size() >= 6) {
            pos = Vec3(v[0], v[1], v[2]);
            look = Vec3(v[3], v[4], v[5]);
            if (v.size() >= 9) up = Vec3(v[6], v[7], v[8]);
            haveLook = true;
        } else {
            Error(e->line, "'lookat' needs 6 or 9 numbers, got " +
                               std::to_string(v.size()));
        }
    }
    if (!haveLook) {
        Error(0, "camera has no 'lookat'; using (0 0 1) -> (0 0 0)");
    }
    double fov = 90.0, aperture = 0.0, focus = 0.0, screenW = 0.0;
    GetNum1(entries, "fov", &fov);
    GetNum1(entries, "focalength", &fov);
    GetNum1(entries, "apertureradius", &aperture);
    GetNum1(entries, "aperture", &aperture);
    GetNum1(entries, "focusdistance", &focus);
    GetNum1(entries, "focaldistance", &focus);
    GetNum1(entries, "screenwidth", &screenW);

    // Only fill in what the Film block (or the defaults) left untouched: the
    // camera block may also carry resolution parameters, and must not undo
    // values a preceding Film block already set.
    if (scene->film.xResolution <= 0) scene->film.xResolution = 1280;
    if (scene->film.yResolution <= 0) scene->film.yResolution = 720;
    scene->film.pixelAspect = 1.0;
    int xr, yr;
    if (GetInt1(entries, "xresolution", &xr) && GetInt1(entries, "yresolution", &yr)) {
        if (xr > 0 && yr > 0) {
            scene->film.xResolution = xr;
            scene->film.yResolution = yr;
        } else {
            Warn(0, "film resolution must be positive; using 1280x720");
        }
    }
    double ar;
    if (GetNum1(entries, "aspectratio", &ar) || GetNum1(entries, "pixelsize", &ar)) {
        if (ar > 0) scene->film.pixelAspect = ar;
    }

    std::string type = ToLower(EntryString(entries, "type"));
    bool ortho = type.find("ortho") != std::string::npos;
    if (!type.empty() && !ortho && type.find("perspective") == std::string::npos) {
        Warn(0, "camera type '" + type + "' not recognised; using perspective");
    }
    if (ortho && screenW <= 0) screenW = 4.0;
    if (ortho) {
        scene->camera.ConfigureOrthographic(screenW, pos, look, up, scene->film);
    } else {
        scene->camera.ConfigurePerspective(Clamp(fov, 1e-3, 179.0), pos, look, up,
                                           scene->film);
        if (aperture > 0)
            scene->camera.ConfigureThinLens(aperture, focus, Clamp(fov, 1e-3, 179.0), pos,
                                            look, up,
                                            scene->film);
    }
    Logger::Instance().Info("camera: " + scene->camera.ProjectionName() +
                            ", fov=" + std::to_string(fov) + " aperture=" +
                            std::to_string(scene->camera.lensRadius) + " focus=" +
                            std::to_string(scene->camera.focusDistance) + " pos=(" +
                            std::to_string(pos.x) + "," + std::to_string(pos.y) + "," +
                            std::to_string(pos.z) + ") look=(" +
                            std::to_string(look.x) + "," + std::to_string(look.y) + "," +
                            std::to_string(look.z) + ") fwd=(" +
                            std::to_string(scene->camera.forward.x) + "," +
                            std::to_string(scene->camera.forward.y) + "," +
                            std::to_string(scene->camera.forward.z) + ")");
}

void SceneReader::ParseIntegrator(const std::string& name, Scene* scene) {
    auto entries = ReadEntries();
    // The integrator is named by the block, e.g. Integrator "distributed" { ... }.
    // pbrt also allows a bare type word inside the block, which wins if present.
    std::string type = ToLower(name);
    for (const auto& e : entries)
        if (e.values.empty() && !IsTypeToken(e.key)) {
            type = e.key;
            break;
        }
    if (type == "distributed" || type == "dr" || type == "rt" || type == "path") {
        scene->integrator = "distributed";
    } else if (type == "whitted" || type == "wsrt" || type == "ray" || type == "rt") {
        scene->integrator = "whitted";
    } else {
        Logger::Instance().Warn("unknown integrator '" + type +
                                "'; using 'whitted'");
        scene->integrator = "whitted";
    }
    int iv;
    if (GetInt1(entries, "maxdepth", &iv)) {
        if (iv < 1) {
            Warn(0, "maxdepth must be >= 1; clamping");
            iv = 1;
        }
        scene->maxDepth = iv;
    }
    if (GetInt1(entries, "branchingfactor", &iv) || GetInt1(entries, "pathsamples", &iv)) {
        if (iv < 1) {
            Warn(0, "branching factor must be >= 1; clamping");
            iv = 1;
        }
        scene->branchingFactor = iv;
    }
    if (GetInt1(entries, "pixelsamples", &iv) || GetInt1(entries, "spp", &iv) ||
        GetInt1(entries, "mindepthsamples", &iv)) {
        if (iv < 1) {
            Warn(0, "samples per pixel must be >= 1; clamping");
            iv = 1;
        }
        scene->spp = iv;
    }
    std::string s = ToLower(EntryString(entries, "sampler"));
    if (!s.empty()) {
        if (s == "uniform" || s == "random")
            scene->sampler = SamplerType::Uniform;
        else if (s == "grid" || s == "stratified" || s == "regular")
            scene->sampler = SamplerType::Grid;
        else if (s == "halton")
            scene->sampler = SamplerType::Halton;
        else if (s == "haltonjittered" || s == "jitteredhalton")
            scene->sampler = SamplerType::HaltonJittered;
        else
            Warn(0, "unknown sampler '" + s + "'; using 'grid'");
    }
    int gn;
    if (GetInt1(entries, "gridsize", &gn) && gn >= 1) scene->gridN = gn;
    double wp;
    if (GetNum1(entries, "whitepoint", &wp) && wp > 0) scene->whitePoint = wp;
    std::string tm = ToLower(EntryString(entries, "tonemap"));
    if (tm == "reinhardluminance" || tm == "luminance")
        scene->perChannelToneMap = false;
    else if (tm == "reinhard" || tm == "perchannel" || tm == "reinhardperchannel")
        scene->perChannelToneMap = true;
    else if (!tm.empty())
        Warn(0, "unknown tone mapping '" + tm + "'; using Reinhard (per channel)");
    std::string g = ToLower(EntryString(entries, "gammacorrect"));
    if (g == "false" || g == "0") scene->gammaCorrect = false;
    if (g == "true" || g == "1") scene->gammaCorrect = true;

    Logger::Instance().Info("integrator '" + scene->integrator + "': maxdepth=" +
                            std::to_string(scene->maxDepth) + " BF=" +
                            std::to_string(scene->branchingFactor) + " spp=" +
                            std::to_string(scene->spp) + " sampler=" +
                            SamplerTypeName(scene->sampler));
}

void SceneReader::ParseFilm(Scene* scene) {
    auto entries = ReadEntries();
    int xr = scene->film.xResolution, yr = scene->film.yResolution;
    GetInt1(entries, "xresolution", &xr);
    GetInt1(entries, "yresolution", &yr);
    if (xr > 0 && yr > 0) {
        scene->film.xResolution = xr;
        scene->film.yResolution = yr;
    } else {
        Warn(0, "film resolution must be positive; using 1280x720");
        scene->film.xResolution = 1280;
        scene->film.yResolution = 720;
    }
    double ar;
    if (GetNum1(entries, "aspectratio", &ar) && ar > 0) scene->film.pixelAspect = ar;
}

bool SceneReader::Parse(Scene* scene) {
    Logger::Instance().Info("scene starts with one default diffuse material");
    scene->materials.push_back(DefaultMaterial());
    SkipSeparators();
    while (!Eof()) {
        Token dirTok = Cur();
        std::string dir = dirTok.text;
        if (dir == "\n" || dir == "," || dir == " ") {
            Next();
            continue;
        }
        if (dir == "EOF") break;
        std::string low = ToLower(dir);
        static const char* kKnown[] = {
            "camera",     "lookat",       "integrator", "film",      "shape",
            "light",      "arealight",    "pointlight", "distantlight",
            "material",   "world",        "object",     "objectinstance",
            "include",    "identity",     "reverseorientation",
            "transform",  "scale",        "translate",  "rotate",    "attribute",
            "pixel",      "option",       "color",      "texture",   "makeNamedMaterial"};
        bool known = false;
        for (const char* k : kKnown)
            if (low == k) known = true;
        if (!known) {
            Error(dirTok, "unknown directive '" + dir + "'");
            while (!Eof() && Cur().text != "\n") Next();
            continue;
        }
        Next();
        SkipSeparators();
        std::string name;
        if (IsQuoted(Cur().text)) {
            name = Unquote(Cur().text);
            Next();
            SkipSeparators();
        }
        if (Cur().text == "{") Next();  // block-form directive

        if (low == "camera" || low == "lookat") {
            ParseCamera(scene);
            continue;
        }
        if (low == "integrator") {
            ParseIntegrator(name, scene);
            continue;
        }
        if (low == "film") {
            ParseFilm(scene);
            continue;
        }
        if (low == "shape") {
            ParseShape(name, scene);
            continue;
        }
        if (low == "light" || low == "arealight" || low == "pointlight" ||
            low == "distantlight") {
            ParseLight(low, low == "light" ? std::string() : name, scene);
            continue;
        }
        if (low == "material" || low == "makenamedmaterial") {
            if (name.empty()) {
                Error(dirTok, "material requires a quoted name");
            } else {
                ParseMaterial(name, scene);
            }
            continue;
        }
        if (low == "world") {
            ReadEntries();
            Logger::Instance().Info("world background accepted (ignored)");
            continue;
        }
        if (low == "object" || low == "objectinstance") {
            Logger::Instance().Warn(
                "'" + low + "' is not supported; the object was skipped");
            ++nWarn;
            ReadEntries();
            continue;
        }
        if (low == "include") {
            Logger::Instance().Warn("'include' is not supported; file '" + name +
                                    "' was not parsed");
            ++nWarn;
            ReadEntries();
            continue;
        }
        // Directives that parse but have no effect on this renderer.
        Logger::Instance().Warn("directive '" + dir + "' is parsed but has no effect");
        ++nWarn;
        ReadEntries();
    }
    scene->numPrimitives = static_cast<int>(scene->shapes.size());
    return nError == 0;
}

}  // namespace

ParseResult ParseSceneFile(const std::string& path) {
    ParseResult res;
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        Logger::Instance().Problem("scene file '" + path + "'", "cannot open file");
        res.nErrors = 1;
        return res;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::string text = ss.str();
    res.nLines = static_cast<int>(std::count(text.begin(), text.end(), '\n')) + 1;

    std::string baseDir = ".";
    size_t slash = path.find_last_of('/');
    if (slash != std::string::npos) baseDir = path.substr(0, slash);

    Logger::Instance().Info("parsing '" + path + "' (" + std::to_string(res.nLines) +
                            " lines)");
    SceneReader reader(Tokenize(text), baseDir);
    // Count everything the Logger records during the parse, so the summary line
    // cannot disagree with the diagnostics written to the log file.
    const int w0 = Logger::Instance().NumWarnings();
    const int e0 = Logger::Instance().NumErrors();
    res.ok = reader.Parse(&res.scene);
    res.nWarnings = Logger::Instance().NumWarnings() - w0;
    res.nErrors = Logger::Instance().NumErrors() - e0;
    res.scene.numPrimitives = static_cast<int>(res.scene.shapes.size());
    return res;
}

}  // namespace cgr
