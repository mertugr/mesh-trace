#pragma once

#include "Vec3.h"
#include <string>
#include <vector>

struct Triangle {
    // 0-based indices (XML is 1-based; loader converts).
    // A value of -1 means "not provided".
    int v0 = -1, v1 = -1, v2 = -1;
    int t0 = -1, t1 = -1, t2 = -1;
    int n0 = -1, n1 = -1, n2 = -1;
    int materialIdx = -1; // resolved at load time

    // Geometric face normal, precomputed for fast backface handling.
    Vec3 faceNormal{0, 0, 1};
};

struct Mesh {
    std::string id;
    int materialIdx = -1;
    // range into Scene::triangles; simplifies BVH over all meshes at once
    int firstTriangle = 0;
    int triangleCount = 0;
};

struct Hit {
    float t = 0.0f;
    Vec3 point{0, 0, 0};
    Vec3 normal{0, 1, 0};     // shading normal (may be smoothed), always facing the ray
    Vec3 faceNormal{0, 1, 0}; // geometric face normal (for offset)
    float b0 = 0, b1 = 0, b2 = 0; // barycentric
    int triangleIdx = -1;
    int materialIdx = -1;
};
