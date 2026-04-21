#pragma once

#include "Vec3.h"
#include <cstdint>
#include <string>
#include <vector>

// 8-bit RGB image, used for both texture maps and the final output buffer.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels; // RGB, row-major, row 0 is top

    void resize(int w, int h) {
        width = w;
        height = h;
        pixels.assign(static_cast<std::size_t>(w) * h * 3, 0);
    }

    void setPixel(int x, int y, const Vec3& color) {
        std::size_t idx = (static_cast<std::size_t>(y) * width + x) * 3;
        pixels[idx + 0] = toByte(color.x);
        pixels[idx + 1] = toByte(color.y);
        pixels[idx + 2] = toByte(color.z);
    }

    // Bilinear sample. (u, v) wraps into [0, 1).
    Vec3 sampleBilinear(float u, float v) const;

    static std::uint8_t toByte(float c) {
        if (c < 0.0f) c = 0.0f;
        if (c > 255.0f) c = 255.0f;
        return static_cast<std::uint8_t>(c + 0.5f);
    }
};

// Read an image from disk. Supports PNG, JPG, BMP, TGA, GIF, PSD, PIC, PNM (P3/P6)
// via stb_image, plus a small custom PPM reader as a last resort. Throws
// std::runtime_error on failure.
Image loadImage(const std::string& path);

// Write an 8-bit RGB image. The file extension picks the encoder:
//   .png -> libpng-free PNG via stb_image_write
//   .bmp -> BMP
//   .tga -> TGA
//   .jpg / .jpeg -> JPEG (quality 90)
//   anything else (or .ppm) -> PPM P6 binary
void writeImage(const std::string& path, const Image& img);

// Kept for backward compatibility with earlier code.
inline Image loadPpm(const std::string& path) { return loadImage(path); }
inline void writePpm(const std::string& path, const Image& img) { writeImage(path, img); }
