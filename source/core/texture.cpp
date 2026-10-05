#include "core/texture.h"

#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>

namespace cgr {

TexturePtr LoadTexturePPM(const std::string& path, std::string* err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (err) *err = "cannot open texture file '" + path + "'";
        return nullptr;
    }
    std::string magic;
    f >> magic;
    if (magic != "P3" && magic != "P6") {
        if (err)
            *err = "texture '" + path + "' is not a PPM file (found magic '" + magic +
                   "'; only P3/P6 are supported)";
        return nullptr;
    }
    auto nextInt = [&](int* v) -> bool {
        int c = f.peek();
        for (;;) {
            if (c == '#') {
                std::string junk;
                std::getline(f, junk);
                c = f.peek();
            } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                f.get();
                c = f.peek();
            } else {
                break;
            }
        }
        if (c == EOF) return false;
        if (!std::isdigit(c)) return false;
        *v = 0;
        while (std::isdigit(c)) {
            *v = *v * 10 + (c - '0');
            f.get();
            c = f.peek();
        }
        return true;
    };
    int w, h, maxv;
    if (!nextInt(&w) || !nextInt(&h) || !nextInt(&maxv)) {
        if (err) *err = "texture '" + path + "' has a truncated header";
        return nullptr;
    }
    if (w <= 0 || h <= 0) {
        if (err) *err = "texture '" + path + "' has non-positive dimensions";
        return nullptr;
    }
    if (maxv != 255) {
        if (err)
            *err = "texture '" + path + "' has maxval " + std::to_string(maxv) +
                   "; only 255 is supported";
        return nullptr;
    }

    auto t = std::make_shared<Texture2D>();
    t->width = w;
    t->height = h;
    t->filename = path;
    t->data.assign(static_cast<size_t>(w) * h * 3, 0.0);
    double scale = 1.0 / maxv;

    if (magic == "P6") {
        f.get();  // the single whitespace after maxval
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                for (int c = 0; c < 3; ++c) {
                    int ch = f.get();
                    if (ch == EOF) {
                        if (err) *err = "texture '" + path + "' has truncated pixel data";
                        return nullptr;
                    }
                    t->Texel(x, y, c) = SRGBToLinear(ch * scale);
                }
            }
        }
    } else {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                for (int c = 0; c < 3; ++c) {
                    int v;
                    if (!nextInt(&v)) {
                        if (err) *err = "texture '" + path + "' has truncated pixel data";
                        return nullptr;
                    }
                    t->Texel(x, y, c) = SRGBToLinear(v * scale);
                }
    }
    return t;
}

namespace {

// Apply the wrap mode to an integer texel coordinate.
int WrapCoord(int i, int n, TextureWrap w) {
    switch (w) {
        case TextureWrap::Repeat:
            return ((i % n) + n) % n;
        case TextureWrap::Clamp:
            return Clamp(i, 0, n - 1);
        case TextureWrap::Black:
            return -1;
    }
    return i;
}

}  // namespace

static Color BilinearImpl(const Texture2D& t, double u, double v, bool nearest) {
    if (!t.IsValid()) return Color(1);
    u *= t.uScale;
    v *= t.vScale;
    if (nearest) {
        int xi = static_cast<int>(std::floor(u * t.width));
        int yi = static_cast<int>(std::floor(v * t.height));
        xi = WrapCoord(xi, t.width, t.wrapS);
        yi = WrapCoord(yi, t.height, t.wrapT);
        if (xi < 0 || yi < 0) return Color(0, 0, 0);
        return Color(t.Texel(xi, yi, 0), t.Texel(xi, yi, 1), t.Texel(xi, yi, 2));
    }
    double fx = u * t.width - 0.5;
    double fy = v * t.height - 0.5;
    int x0 = static_cast<int>(std::floor(fx));
    int y0 = static_cast<int>(std::floor(fy));
    double dx = fx - x0, dy = fy - y0;
    double out[3] = {0, 0, 0};
    for (int j = 0; j < 2; ++j) {
        for (int i = 0; i < 2; ++i) {
            int xi = WrapCoord(x0 + i, t.width, t.wrapS);
            int yi = WrapCoord(y0 + j, t.height, t.wrapT);
            if (xi < 0 || yi < 0) continue;  // black texel in the fringe
            double w = (i ? dx : 1 - dx) * (j ? dy : 1 - dy);
            for (int c = 0; c < 3; ++c) out[c] += w * t.Texel(xi, yi, c);
        }
    }
    return Color(out[0], out[1], out[2]);
}

Color TextureLookup(const Texture2D& t, double u, double v) {
    return BilinearImpl(t, u, v, false);
}

Color TextureLookupNearest(const Texture2D& t, double u, double v) {
    return BilinearImpl(t, u, v, true);
}

TexturePtr WhiteTexture() {
    static TexturePtr tex = [] {
        auto t = std::make_shared<Texture2D>();
        t->width = t->height = 1;
        t->data = {1.0, 1.0, 1.0};
        return t;
    }();
    return tex;
}

}  // namespace cgr
