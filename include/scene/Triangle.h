#pragma once

#include "core/Intersection.h"
#include "core/Ray.h"
#include "math/Vec2.h"
#include "math/Vec3.h"

namespace rt {

struct Triangle {
    Vec3 v0;
    Vec3 v1;
    Vec3 v2;

    Vec3 n0;
    Vec3 n1;
    Vec3 n2;

    Vec2 uv0;
    Vec2 uv1;
    Vec2 uv2;

    int materialId{-1};

    bool intersect(const Ray& ray, Intersection& isect) const;
};

} // namespace rt
