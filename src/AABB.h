#pragma once

#include "Ray.h"
#include "Vec3.h"

#include <algorithm>
#include <limits>

struct AABB {
    Vec3 bmin{std::numeric_limits<float>::infinity(),
              std::numeric_limits<float>::infinity(),
              std::numeric_limits<float>::infinity()};
    Vec3 bmax{-std::numeric_limits<float>::infinity(),
              -std::numeric_limits<float>::infinity(),
              -std::numeric_limits<float>::infinity()};

    void expand(const Vec3& p) {
        bmin = bmin.cwiseMin(p);
        bmax = bmax.cwiseMax(p);
    }
    void expand(const AABB& b) {
        bmin = bmin.cwiseMin(b.bmin);
        bmax = bmax.cwiseMax(b.bmax);
    }
    Vec3 centroid() const { return (bmin + bmax) * 0.5f; }
    Vec3 extent() const { return bmax - bmin; }
    int longestAxis() const {
        Vec3 e = extent();
        if (e.x >= e.y && e.x >= e.z) return 0;
        if (e.y >= e.z) return 1;
        return 2;
    }

    // Slab test. Returns true if ray parameter range overlaps [tMin, tMax].
    // Uses strict `<` so that a flat (zero-thickness) AABB — e.g. the bbox of
    // a single planar quad like a floor — still registers a grazing hit.
    bool intersect(const Ray& r, const Vec3& invDir, float tMin, float tMax) const {
        for (int a = 0; a < 3; ++a) {
            float t0 = (bmin[a] - r.origin[a]) * invDir[a];
            float t1 = (bmax[a] - r.origin[a]) * invDir[a];
            if (invDir[a] < 0.0f) std::swap(t0, t1);
            tMin = t0 > tMin ? t0 : tMin;
            tMax = t1 < tMax ? t1 : tMax;
            if (tMax < tMin) return false;
        }
        return true;
    }
};
