#include "core/image.h"

#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>

#include "core/logger.h"

namespace cgr {

//
// Tone mapping (Reinhard). Two variants, documented in the report:
//   * per-channel  : c / (1 + c) independently per channel
//   * luminance    : scale the colour so its luminance is compressed, which
//                    preserves hue
// The white point W replaces 1 with W:  c * (1 + c/W^2) / (1 + c)
//
Color ToneMapReinhard(Color c, double whitePoint, bool perChannel) {
    if (perChannel) {
        auto map = [&](double x) {
            if (x < 0) return 0.0;
            return (x * (1.0 + x / (whitePoint * whitePoint))) / (1.0 + x);
        };
        return Color(map(c.r), map(c.g), map(c.b));
    }
    double y = Luminance(c);
    if (y <= 0) return Color(0);
    double ym = (y * (1.0 + y / (whitePoint * whitePoint))) / (1.0 + y);
    double s = ym / y;
    return c * s;
}

Color ToneMapReinhardLuminance(Color c, double wp) { return ToneMapReinhard(c, wp, false); }

namespace {

// Encode a linear image to 8-bit, applying the tone map first, then sRGB.
std::vector<unsigned char> ToBytes(const Image& img, bool srgbEncode, bool toneMap,
                                    double whitePoint, bool perChannel) {
    std::vector<unsigned char> out;
    out.reserve(img.pixels.size());
    for (double p : img.pixels) {
        double v = p;
        if (toneMap) {
            Color t = ToneMapReinhard(Color(v), whitePoint, perChannel);
            v = t.r;
        }
        v = Clamp(v, 0.0, 1.0);
        if (srgbEncode) v = LinearToSRGB(v);
        out.push_back(static_cast<unsigned char>(Clamp(v * 255.0 + 0.5, 0.0, 255.0)));
    }
    return out;
}

void AppendBE32(std::string& s, std::uint32_t v) {
    s.push_back(static_cast<char>((v >> 24) & 0xff));
    s.push_back(static_cast<char>((v >> 16) & 0xff));
    s.push_back(static_cast<char>((v >> 8) & 0xff));
    s.push_back(static_cast<char>(v & 0xff));
}

std::uint32_t Crc32(const std::string& data) {
    static std::uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xedb88320u ^ (c >> 1) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    std::uint32_t c = 0xffffffffu;
    for (unsigned char ch : data) c = table[(c ^ ch) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

void AppendChunk(std::string& out, const char* type, const std::string& data) {
    AppendBE32(out, static_cast<std::uint32_t>(data.size()));
    std::string body(type, 4);
    body += data;
    out += body;
    AppendBE32(out, Crc32(body));
}

// zlib stream using stored (uncompressed) deflate blocks: valid zlib, no
// external compression library needed. PNG files stay a few hundred kB.
std::string ZlibStore(const std::string& raw) {
    std::string out;
    out.push_back(0x78);  // CMF: deflate, 32k window
    out.push_back(0x01);  // FLG: no dict, fastest; (0x78<<8|0x01) % 31 == 0
    size_t pos = 0;
    if (raw.empty()) {
        out.push_back(0x01);
        out.push_back(0x00);
        out.push_back(0x00);
        out.push_back(0xff);
        out.push_back(0xff);
    }
    while (pos < raw.size()) {
        size_t n = std::min<size_t>(65535, raw.size() - pos);
        bool last = (pos + n == raw.size());
        out.push_back(last ? 1 : 0);
        out.push_back(static_cast<char>(n & 0xff));
        out.push_back(static_cast<char>((n >> 8) & 0xff));
        out.push_back(static_cast<char>(~n & 0xff));
        out.push_back(static_cast<char>((~n >> 8) & 0xff));
        out.append(raw, pos, n);
        pos += n;
    }
    std::uint32_t a = 1, b = 0;
    for (unsigned char ch : raw) {
        a = (a + ch) % 65521;
        b = (b + a) % 65521;
    }
    AppendBE32(out, (b << 16) | a);
    return out;
}

}  // namespace

bool WritePPM(const Image& img, const std::string& path, bool srgbEncode) {
    // PPM output is display-referred, so tone map is assumed already applied by
    // the integrator. We only gamma encode here.
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        LOGE("WritePPM: cannot open " + path);
        return false;
    }
    std::ostringstream hdr;
    hdr << "P6\n# CGR26 renderer\n" << img.xSize << " " << img.ySize << "\n255\n";
    std::string h = hdr.str();
    f.write(h.data(), static_cast<std::streamsize>(h.size()));
    std::vector<unsigned char> bytes = ToBytes(img, srgbEncode, false, 1.0, true);
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    return f.good();
}

bool WritePNG(const Image& img, const std::string& path, bool srgbEncode) {
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        LOGE("WritePNG: cannot open " + path);
        return false;
    }
    static const unsigned char kSig[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};

    std::string out;
    out.append(reinterpret_cast<const char*>(kSig), 8);

    std::string chunk;
    AppendBE32(chunk, static_cast<std::uint32_t>(img.xSize));
    AppendBE32(chunk, static_cast<std::uint32_t>(img.ySize));
    chunk.push_back(8);                           // bit depth
    chunk.push_back(img.nChannels >= 3 ? 2 : 0);  // truecolour / greyscale
    chunk.push_back(0);                           // compression: deflate
    chunk.push_back(0);                           // adaptive filtering
    chunk.push_back(0);                           // no interlace
    AppendChunk(out, "IHDR", chunk);

    // Raw scanlines, each prefixed with filter type 0 (None).
    std::vector<unsigned char> bytes = ToBytes(img, srgbEncode, false, 1.0, true);
    std::string raw;
    raw.reserve(static_cast<size_t>(img.ySize) *
                (1 + static_cast<size_t>(img.xSize) * img.nChannels));
    for (int y = 0; y < img.ySize; ++y) {
        raw.push_back(0);
        size_t off = static_cast<size_t>(y) * img.xSize * img.nChannels;
        raw.append(reinterpret_cast<const char*>(bytes.data() + off),
                   static_cast<size_t>(img.xSize) * img.nChannels);
    }
    AppendChunk(out, "IDAT", ZlibStore(raw));
    AppendChunk(out, "IEND", std::string());
    f.write(out.data(), static_cast<std::streamsize>(out.size()));
    return f.good();
}

bool ReadPPM(const std::string& path, Image* out, std::string* err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (err) *err = "cannot open file";
        return false;
    }
    auto fail = [&](const std::string& m) {
        if (err) *err = m;
        return false;
    };
    std::string magic;
    f >> magic;
    if (magic != "P3" && magic != "P6") return fail("not a PPM file (bad magic '" + magic + "')");
    auto readInt = [&](int* v) {
        int c = f.peek();
        while (c == '#' || c == ' ' || c == '\n' || c == '\r' || c == '\t') {
            if (c == '#') {
                std::string junk;
                std::getline(f, junk);
            } else {
                f.get();
            }
            c = f.peek();
        }
        if (c == EOF) return false;
        *v = 0;
        while (std::isdigit(c)) {
            *v = *v * 10 + (c - '0');
            f.get();
            c = f.peek();
        }
        return true;
    };
    int w, h, maxv;
    if (!readInt(&w) || !readInt(&h) || !readInt(&maxv))
        return fail("truncated PPM header");
    if (maxv != 255) return fail("only maxval 255 PPM files are supported");
    *out = Image(w, h, 3);
    if (magic == "P6") {
        f.get();  // single whitespace after maxval
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                for (int c = 0; c < 3; ++c) {
                    int ch = f.get();
                    if (ch == EOF) return fail("truncated PPM data");
                    out->operator()(x, y, c) = SRGBToLinear(ch / 255.0);
                }
            }
        }
    } else {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                for (int c = 0; c < 3; ++c) {
                    int v;
                    if (!readInt(&v)) return fail("truncated PPM data");
                    out->operator()(x, y, c) = SRGBToLinear(v / 255.0);
                }
    }
    return true;
}

bool WriteImage(const Image& img, const std::string& path, ImageFormat fmt,
                bool srgbEncode) {
    return fmt == ImageFormat::PNG ? WritePNG(img, path, srgbEncode)
                                   : WritePPM(img, path, srgbEncode);
}

ImageStats CompareImages(const Image& a, const Image& b) {
    ImageStats s;
    s.nTotal = static_cast<long long>(a.xSize) * a.ySize;
    if (a.xSize != b.xSize || a.ySize != b.ySize || a.nChannels != b.nChannels) return s;
    double se = 0, maxE = 0, sumRef[3] = {0, 0, 0}, sumErr[3] = {0, 0, 0}, sumPct = 0;
    for (int y = 0; y < a.ySize; ++y) {
        for (int x = 0; x < a.xSize; ++x) {
            Color ca = a.GetPixel(x, y), cb = b.GetPixel(x, y);
            double refMag = 0, pct = 0;
            bool diff = false;
            for (int c = 0; c < 3; ++c) {
                double d = ca[c] - cb[c];
                se += d * d;
                maxE = std::max(maxE, std::fabs(d));
                sumRef[c] += cb[c];
                sumErr[c] += d;
                refMag = std::max(refMag, std::fabs(cb[c]));
                if (std::fabs(d) > 1.0 / 255.0) diff = true;
            }
            if (refMag > 1e-6) {
                double m = 0;
                for (int c = 0; c < 3; ++c) m = std::max(m, std::fabs(ca[c] - cb[c]));
                pct = 100.0 * m / refMag;
                sumPct += pct;
            }
            if (diff) ++s.nDiffering;
        }
    }
    if (s.nTotal > 0) {
        s.rmse = std::sqrt(se / (3.0 * s.nTotal));
        s.maxAbsErr = maxE;
        for (int c = 0; c < 3; ++c) {
            s.meanRef[c] = sumRef[c] / s.nTotal;
            s.meanErr[c] = sumErr[c] / s.nTotal;
        }
        s.meanAbsPct = sumPct / s.nTotal;
        s.psnr = (s.rmse > 0) ? 20.0 * std::log10(1.0 / s.rmse) : 0.0;
    }
    return s;
}

double DifferingPixelFraction(const Image& a, const Image& b, double tol) {
    if (a.xSize != b.xSize || a.ySize != b.ySize) return 1.0;
    long long n = 0, tot = 0;
    for (int y = 0; y < a.ySize; ++y)
        for (int x = 0; x < a.xSize; ++x) {
            ++tot;
            Color ca = a.GetPixel(x, y), cb = b.GetPixel(x, y);
            double m = 0;
            for (int c = 0; c < 3; ++c) m = std::max(m, std::fabs(ca[c] - cb[c]));
            if (m > tol) ++n;
        }
    return tot ? static_cast<double>(n) / tot : 0.0;
}

}  // namespace cgr
