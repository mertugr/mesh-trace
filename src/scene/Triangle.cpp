#include "scene/Triangle.h"

#include <cmath>

namespace rt {

bool Triangle::intersect(const Ray& ray, Intersection& isect) const {
    const Vec3 e1 = v1 - v0;
    const Vec3 e2 = v2 - v0;

    const Vec3 pvec = Vec3::cross(ray.direction, e2);
    const double det = Vec3::dot(e1, pvec);
    if (std::abs(det) < 1e-10) {
        return false;
    }

    const double invDet = 1.0 / det;
    const Vec3 tvec = ray.origin - v0;
    const double u = Vec3::dot(tvec, pvec) * invDet;
    if (u < 0.0 || u > 1.0) {
        return false;
    }

    const Vec3 qvec = Vec3::cross(tvec, e1);
    const double v = Vec3::dot(ray.direction, qvec) * invDet;
    if (v < 0.0 || u + v > 1.0) {
        return false;
    }

    const double t = Vec3::dot(e2, qvec) * invDet;
    if (t < ray.tMin || t > ray.tMax || t >= isect.t) {
        return false;
    }

    const double w = 1.0 - u - v;
    Vec3 normal = (n0 * w + n1 * u + n2 * v).normalized();
    if (normal.lengthSquared() == 0.0) {
        normal = Vec3::cross(e1, e2).normalized();
    }

    const Vec2 uv = {
        uv0.x * w + uv1.x * u + uv2.x * v,
        uv0.y * w + uv1.y * u + uv2.y * v,
    };

    isect.hit = true;
    isect.t = t;
    isect.position = ray.at(t);
    isect.normal = normal;
    isect.uv = uv;
    isect.triangle = this;
    return true;
}

} // namespace rt
