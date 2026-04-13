#include "scene/Texture.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace rt {
namespace {

double fract(double x) {
    return x - std::floor(x);
}

Vec3 sampleTexel(const std::vector<uint8_t>& data, int width, int height, int channels, int x, int y) {
    const int sx = std::clamp(x, 0, width - 1);
    const int sy = std::clamp(y, 0, height - 1);
    const size_t idx = static_cast<size_t>((sy * width + sx) * channels);
    if (idx >= data.size()) {
        return {1.0, 1.0, 1.0};
    }

    if (channels <= 2) {
        const double c = data[idx] / 255.0;
        return {c, c, c};
    }

    if (idx + 2 >= data.size()) {
        return {1.0, 1.0, 1.0};
    }

    return {
        data[idx] / 255.0,
        data[idx + 1] / 255.0,
        data[idx + 2] / 255.0,
    };
}

} // namespace

bool ImageTexture::load(const std::string& path) {
    int w = 0;
    int h = 0;
    int ch = 0;
    uint8_t* pixels = stbi_load(path.c_str(), &w, &h, &ch, 0);
    if (pixels == nullptr) {
        return false;
    }

    width_ = w;
    height_ = h;
    channels_ = ch;
    const size_t size = static_cast<size_t>(w) * static_cast<size_t>(h) * static_cast<size_t>(ch);
    data_.assign(pixels, pixels + size);
    stbi_image_free(pixels);
    return true;
}

Vec3 ImageTexture::sample(const Vec2& uv) const {
    if (data_.empty() || width_ <= 0 || height_ <= 0 || channels_ <= 0) {
        return {1.0, 1.0, 1.0};
    }

    const double u = fract(uv.x < 0.0 ? uv.x + std::ceil(-uv.x) : uv.x);
    const double v = fract(uv.y < 0.0 ? uv.y + std::ceil(-uv.y) : uv.y);
    const double px = u * static_cast<double>(width_ - 1);
    const double py = (1.0 - v) * static_cast<double>(height_ - 1);

    const int x0 = static_cast<int>(std::floor(px));
    const int y0 = static_cast<int>(std::floor(py));
    const int x1 = std::min(x0 + 1, width_ - 1);
    const int y1 = std::min(y0 + 1, height_ - 1);

    const double tx = px - static_cast<double>(x0);
    const double ty = py - static_cast<double>(y0);

    const Vec3 c00 = sampleTexel(data_, width_, height_, channels_, x0, y0);
    const Vec3 c10 = sampleTexel(data_, width_, height_, channels_, x1, y0);
    const Vec3 c01 = sampleTexel(data_, width_, height_, channels_, x0, y1);
    const Vec3 c11 = sampleTexel(data_, width_, height_, channels_, x1, y1);

    const Vec3 c0 = c00 * (1.0 - tx) + c10 * tx;
    const Vec3 c1 = c01 * (1.0 - tx) + c11 * tx;
    return c0 * (1.0 - ty) + c1 * ty;
}

} // namespace rt
