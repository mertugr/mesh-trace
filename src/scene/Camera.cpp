#include "scene/Camera.h"

#include "math/Vec3.h"

namespace rt {

void Camera::prepare() {
    const Vec3 g = gaze.normalized();
    u_ = Vec3::cross(g, up).normalized();
    v_ = Vec3::cross(u_, g).normalized();
    w_ = g;
}

Ray Camera::generateRay(int px, int py) const {
    const double su = left + (right - left) * (static_cast<double>(px) + 0.5) / static_cast<double>(imageWidth);
    const double sv = top - (top - bottom) * (static_cast<double>(py) + 0.5) / static_cast<double>(imageHeight);

    const Vec3 dir = (w_ * nearDistance + u_ * su + v_ * sv).normalized();
    return Ray(position, dir);
}

} // namespace rt
