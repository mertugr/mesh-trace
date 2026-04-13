#pragma once

#include <string>
#include <vector>

#include "math/Vec3.h"

namespace rt {

class Image {
public:
    Image(int w, int h);

    int width() const { return width_; }
    int height() const { return height_; }

    void setPixel(int x, int y, const Vec3& color);
    Vec3 getPixel(int x, int y) const;
    bool savePPM(const std::string& filePath) const;
    bool savePNG(const std::string& filePath) const;

private:
    int width_;
    int height_;
    std::vector<Vec3> pixels_;
};

} // namespace rt
