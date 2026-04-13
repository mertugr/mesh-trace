#pragma once

#include "math/Vec3.h"

namespace rt {

struct Material {
    int id{-1};
    Vec3 ambient{0.0, 0.0, 0.0};
    Vec3 diffuse{0.0, 0.0, 0.0};
    Vec3 specular{0.0, 0.0, 0.0};
    Vec3 mirrorReflectance{0.0, 0.0, 0.0};
    double phongExponent{1.0};
    double textureFactor{0.0};
};

} // namespace rt
