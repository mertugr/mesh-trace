#pragma once

#include <vector>

#include "core/Intersection.h"
#include "core/Ray.h"
#include "math/Vec3.h"
#include "scene/Triangle.h"

namespace rt {

struct AABB {
    Vec3 mn{1e30, 1e30, 1e30};
    Vec3 mx{-1e30, -1e30, -1e30};

    void expand(const Vec3& p) {
        mn.x = std::min(mn.x, p.x);
        mn.y = std::min(mn.y, p.y);
        mn.z = std::min(mn.z, p.z);
        mx.x = std::max(mx.x, p.x);
        mx.y = std::max(mx.y, p.y);
        mx.z = std::max(mx.z, p.z);
    }

    void expand(const AABB& other) {
        expand(other.mn);
        expand(other.mx);
    }

    double surfaceArea() const {
        const double dx = mx.x - mn.x;
        const double dy = mx.y - mn.y;
        const double dz = mx.z - mn.z;
        return 2.0 * (dx * dy + dy * dz + dz * dx);
    }

    bool intersect(const Vec3& invDir, const Vec3& origin, double tMin, double tMax) const {
        double t0x = (mn.x - origin.x) * invDir.x;
        double t1x = (mx.x - origin.x) * invDir.x;
        if (invDir.x < 0.0) { std::swap(t0x, t1x); }

        double t0y = (mn.y - origin.y) * invDir.y;
        double t1y = (mx.y - origin.y) * invDir.y;
        if (invDir.y < 0.0) { std::swap(t0y, t1y); }

        double t0z = (mn.z - origin.z) * invDir.z;
        double t1z = (mx.z - origin.z) * invDir.z;
        if (invDir.z < 0.0) { std::swap(t0z, t1z); }

        const double tEnter = std::max({t0x, t0y, t0z});
        const double tExit = std::min({t1x, t1y, t1z});
        return tEnter <= tExit && tExit >= tMin && tEnter <= tMax;
    }
};

struct BVHNode {
    AABB bounds;
    int leftChild{-1};
    int rightChild{-1};
    int triStart{-1};
    int triCount{0};

    bool isLeaf() const { return triCount > 0; }
};

class BVH {
public:
    void build(std::vector<Triangle>& triangles);
    bool intersect(const Ray& ray, const std::vector<Triangle>& triangles, Intersection& closest) const;

private:
    int buildRecursive(std::vector<Triangle>& triangles, int start, int count);

    std::vector<BVHNode> nodes_;
};

} // namespace rt
