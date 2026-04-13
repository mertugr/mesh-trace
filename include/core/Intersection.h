#pragma once

#include "math/Vec2.h"
#include "math/Vec3.h"

namespace rt {

struct Triangle;

struct Intersection {
    bool hit{false};
    double t{1e30};
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    const Triangle* triangle{nullptr};
};

} // namespace rt
