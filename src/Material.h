#pragma once

#include "Vec3.h"
#include <string>

struct Material {
    std::string id;
    Vec3 ambient{1, 1, 1};
    Vec3 diffuse{1, 1, 1};
    Vec3 specular{1, 1, 1};
    Vec3 mirrorReflectance{0, 0, 0};
    float phongExponent = 1.0f;
    float textureFactor = 0.0f;

    bool isMirror() const {
        return mirrorReflectance.x > 0 || mirrorReflectance.y > 0 || mirrorReflectance.z > 0;
    }
};
