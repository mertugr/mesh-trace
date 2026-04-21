#pragma once

#include "Vec3.h"
#include <string>

struct AmbientLight {
    Vec3 intensity{0, 0, 0};
};

struct PointLight {
    std::string id;
    Vec3 position{0, 0, 0};
    Vec3 intensity{0, 0, 0};
};

// Planar directional light defined by a triangle.
// Emission direction is normalized cross((v1 - v2), (v1 - v3)).
// We treat the centroid as the sampling point for deterministic images.
struct TriangularLight {
    std::string id;
    Vec3 v1, v2, v3;
    Vec3 intensity{0, 0, 0};
    Vec3 direction{0, 1, 0}; // unit vector, computed at load time
    Vec3 centroid{0, 0, 0};
    float area = 0.0f;
};
