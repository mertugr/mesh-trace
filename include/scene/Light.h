#pragma once

#include <cmath>
#include <limits>

#include "math/Vec3.h"

namespace rt {

class Light {
public:
    virtual ~Light() = default;

    // Samples a light for a surface point:
    // L: normalized direction from point to light source
    // maxDistance: upper t bound for shadow ray tests
    // radiance: RGB light contribution for this sample
    virtual bool sample(const Vec3& point, Vec3& L, double& maxDistance, Vec3& radiance) const = 0;
};

class PointLight final : public Light {
public:
    PointLight(const Vec3& pos, const Vec3& inten) : position_(pos), intensity_(inten) {}
    bool sample(const Vec3& point, Vec3& L, double& maxDistance, Vec3& radiance) const override {
        const Vec3 toLight = position_ - point;
        const double dist2 = toLight.lengthSquared();
        if (dist2 <= 1e-12) {
            return false;
        }

        const double dist = std::sqrt(dist2);
        L = toLight / dist;
        maxDistance = dist;
        radiance = intensity_ / dist2;
        return true;
    }

private:
    Vec3 position_;
    Vec3 intensity_;
};

class TriangularLight final : public Light {
public:
    TriangularLight(const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& inten)
        : v0_(v0), v1_(v1), v2_(v2), intensity_(inten) {
        // Assignment definition: direction follows (v1-v2) x (v1-v3)
        direction_ = Vec3::cross(v0_ - v1_, v0_ - v2_).normalized();
    }

    bool sample(const Vec3&, Vec3& L, double& maxDistance, Vec3& radiance) const override {
        if (direction_.lengthSquared() <= 1e-12) {
            return false;
        }

        // Light travels along "direction_"; shading uses point->light direction.
        L = direction_ * -1.0;
        maxDistance = std::numeric_limits<double>::max();
        radiance = intensity_;
        return true;
    }

private:
    Vec3 v0_;
    Vec3 v1_;
    Vec3 v2_;
    Vec3 intensity_;
    Vec3 direction_{0.0, 0.0, 0.0};
};

} // namespace rt
