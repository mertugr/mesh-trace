#pragma once

#include <cmath>

namespace rt {

struct Vec2 {
    double x{0.0};
    double y{0.0};

    Vec2() = default;
    Vec2(double x_, double y_) : x(x_), y(y_) {}
};

} // namespace rt
