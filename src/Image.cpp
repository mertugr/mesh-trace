#include "Image.h"

#include "stb_image.h"
#include "stb_image_write.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string extensionLower(const std::string& path) {
    std::size_t slash = path.find_last_of("/\\");
    std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return {};
    return lower(path.substr(dot + 1));
}

void skipWhitespaceAndComments(std::istream& is) {
    int c;
    while ((c = is.peek()) != EOF) {
        if (std::isspace(c)) {
            is.get();
        } else if (c == '#') {
            std::string line;
            std::getline(is, line);
        } else {
            break;
        }
    }
}

// Manual PPM P3/P6 loader, used as a fallback and for the textures generated
// by tools/gen_assets.py even when stb cannot handle them.
Image loadPpmFallback(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open image: " + path);

    std::string magic;
    f >> magic;
    if (magic != "P3" && magic != "P6") {
        throw std::runtime_error("unsupported PPM magic (" + magic + "): " + path);
    }

    skipWhitespaceAndComments(f);
    int w = 0, h = 0, maxVal = 255;
    f >> w;
    skipWhitespaceAndComments(f);
    f >> h;
    skipWhitespaceAndComments(f);
    f >> maxVal;

    if (w <= 0 || h <= 0 || maxVal <= 0) {
        throw std::runtime_error("invalid PPM header: " + path);
    }
    f.get(); // consume the single whitespace separator

    Image img;
    img.resize(w, h);

    if (magic == "P6") {
        if (maxVal == 255) {
            f.read(reinterpret_cast<char*>(img.pixels.data()),
                   static_cast<std::streamsize>(img.pixels.size()));
            if (!f) throw std::runtime_error("truncated PPM: " + path);
        } else {
            std::vector<unsigned char> buf(img.pixels.size());
            f.read(reinterpret_cast<char*>(buf.data()),
                   static_cast<std::streamsize>(buf.size()));
            if (!f) throw std::runtime_error("truncated PPM: " + path);
            for (std::size_t i = 0; i < buf.size(); ++i) {
                img.pixels[i] = static_cast<std::uint8_t>(buf[i] * 255 / maxVal);
            }
        }
    } else { // P3
        for (std::size_t i = 0; i < img.pixels.size(); ++i) {
            int v;
            if (!(f >> v)) throw std::runtime_error("truncated PPM: " + path);
            if (maxVal != 255) v = v * 255 / maxVal;
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            img.pixels[i] = static_cast<std::uint8_t>(v);
        }
    }

    return img;
}

void writePpmBinary(const std::string& path, const Image& img) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot write PPM: " + path);
    f << "P6\n" << img.width << ' ' << img.height << "\n255\n";
    f.write(reinterpret_cast<const char*>(img.pixels.data()),
            static_cast<std::streamsize>(img.pixels.size()));
    if (!f) throw std::runtime_error("failed writing PPM: " + path);
}

} // namespace

Vec3 Image::sampleBilinear(float u, float v) const {
    if (width <= 0 || height <= 0) return Vec3{0, 0, 0};

    // Wrap into [0, 1).
    u = u - std::floor(u);
    v = v - std::floor(v);

    // Flip v so (u=0, v=0) is bottom-left.
    float y = (1.0f - v) * height - 0.5f;
    float x = u * width - 0.5f;

    int x0 = static_cast<int>(std::floor(x));
    int y0 = static_cast<int>(std::floor(y));
    float fx = x - x0;
    float fy = y - y0;

    auto wrap = [](int i, int n) {
        i = i % n;
        if (i < 0) i += n;
        return i;
    };
    int x1 = wrap(x0 + 1, width);
    int y1 = wrap(y0 + 1, height);
    x0 = wrap(x0, width);
    y0 = wrap(y0, height);

    auto fetch = [&](int px, int py) -> Vec3 {
        std::size_t idx = (static_cast<std::size_t>(py) * width + px) * 3;
        return Vec3{static_cast<float>(pixels[idx + 0]),
                    static_cast<float>(pixels[idx + 1]),
                    static_cast<float>(pixels[idx + 2])};
    };

    Vec3 c00 = fetch(x0, y0);
    Vec3 c10 = fetch(x1, y0);
    Vec3 c01 = fetch(x0, y1);
    Vec3 c11 = fetch(x1, y1);

    Vec3 cx0 = c00 * (1 - fx) + c10 * fx;
    Vec3 cx1 = c01 * (1 - fx) + c11 * fx;
    return cx0 * (1 - fy) + cx1 * fy;
}

Image loadImage(const std::string& path) {
    int w = 0, h = 0, comp = 0;
    // Force RGB. stb_image accepts PNG, JPG, BMP, TGA, GIF, PSD, PIC, and PNM.
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 3);
    if (!data) {
        // Fall back to our manual PPM reader in case stb rejects unusual PPM
        // variants or the path is otherwise weird.
        try {
            return loadPpmFallback(path);
        } catch (...) {
            std::string why = stbi_failure_reason() ? stbi_failure_reason() : "unknown";
            throw std::runtime_error("cannot decode image '" + path + "': " + why);
        }
    }
    Image img;
    img.resize(w, h);
    std::memcpy(img.pixels.data(), data, static_cast<std::size_t>(w) * h * 3);
    stbi_image_free(data);
    return img;
}

void writeImage(const std::string& path, const Image& img) {
    std::string ext = extensionLower(path);
    int result = 0;
    if (ext == "png") {
        result = stbi_write_png(path.c_str(), img.width, img.height, 3,
                                img.pixels.data(), img.width * 3);
    } else if (ext == "bmp") {
        result = stbi_write_bmp(path.c_str(), img.width, img.height, 3,
                                img.pixels.data());
    } else if (ext == "tga") {
        result = stbi_write_tga(path.c_str(), img.width, img.height, 3,
                                img.pixels.data());
    } else if (ext == "jpg" || ext == "jpeg") {
        result = stbi_write_jpg(path.c_str(), img.width, img.height, 3,
                                img.pixels.data(), 90);
    } else {
        writePpmBinary(path, img);
        return;
    }
    if (!result) throw std::runtime_error("failed to write image: " + path);
}
