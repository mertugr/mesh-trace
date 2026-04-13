#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "math/Vec2.h"
#include "math/Vec3.h"

namespace rt {

class Texture {
public:
    virtual ~Texture() = default;
    virtual Vec3 sample(const Vec2& uv) const = 0;
};

class ImageTexture final : public Texture {
public:
    bool load(const std::string& path);
    Vec3 sample(const Vec2& uv) const override;
    bool empty() const { return data_.empty(); }

private:
    int width_{0};
    int height_{0};
    int channels_{0};
    std::vector<uint8_t> data_;
};

} // namespace rt
