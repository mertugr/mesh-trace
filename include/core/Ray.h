#pragma once

#include "math/Vec3.h"

namespace rt {

struct Ray {
    Vec3 origin;
    Vec3 direction;
    double tMin{1e-6};
    double tMax{1e30};

    Ray() = default;
    Ray(const Vec3& o, const Vec3& d, double mn = 1e-6, double mx = 1e30) : origin(o), direction(d), tMin(mn), tMax(mx) {}

    Vec3 at(double t) const {
        return origin + direction * t;
    }
};

} // namespace rt
