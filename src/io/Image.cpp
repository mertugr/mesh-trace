#include "io/Image.h"

#include <cstdint>
#include <fstream>
#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace rt {

Image::Image(int w, int h) : width_(w), height_(h), pixels_(static_cast<size_t>(w * h), Vec3(0.0, 0.0, 0.0)) {}

void Image::setPixel(int x, int y, const Vec3& color) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }
    pixels_[static_cast<size_t>(y * width_ + x)] = color;
}

bool Image::savePPM(const std::string& filePath) const {
    std::ofstream out(filePath, std::ios::binary);
    if (out.fail()) {
        return false;
    }

    out << "P6\n" << width_ << " " << height_ << "\n255\n";

    for (const Vec3& c : pixels_) {
        const Vec3 clamped = Vec3::clamp(c, 0.0, 255.0);
        const uint8_t rgb[3] = {
            static_cast<uint8_t>(clamped.x),
            static_cast<uint8_t>(clamped.y),
            static_cast<uint8_t>(clamped.z),
        };
        out.write(reinterpret_cast<const char*>(rgb), 3);
    }

    return out.good();
}

bool Image::savePNG(const std::string& filePath) const {
    std::vector<uint8_t> rgb(static_cast<size_t>(width_ * height_ * 3));
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const Vec3 c = Vec3::clamp(pixels_[static_cast<size_t>(y * width_ + x)], 0.0, 255.0);
            const size_t idx = static_cast<size_t>((y * width_ + x) * 3);
            rgb[idx] = static_cast<uint8_t>(c.x);
            rgb[idx + 1] = static_cast<uint8_t>(c.y);
            rgb[idx + 2] = static_cast<uint8_t>(c.z);
        }
    }

    const int strideInBytes = width_ * 3;
    return stbi_write_png(filePath.c_str(), width_, height_, 3, rgb.data(), strideInBytes) != 0;
}

} // namespace rt
