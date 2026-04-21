#pragma once

#include "Vec3.h"

struct Ray {
    Vec3 origin;
    Vec3 direction;
    int depth = 0;

    Ray() = default;
    Ray(const Vec3& o, const Vec3& d, int depth_ = 0)
        : origin(o), direction(d), depth(depth_) {}

    Vec3 at(float t) const { return origin + direction * t; }
};
