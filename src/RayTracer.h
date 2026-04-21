#pragma once

#include "BVH.h"
#include "Image.h"
#include "Ray.h"
#include "Scene.h"
#include "Vec3.h"

class RayTracer {
public:
    explicit RayTracer(const Scene& scene);

    // Renders the whole camera view into `out` (resized to camera resolution).
    // Uses std::thread::hardware_concurrency() threads by default.
    void render(Image& out, int threadCount = 0);

    // Shadow offset added along the surface normal to avoid self-intersection.
    static constexpr float kShadowEpsilon = 1e-3f;

private:
    const Scene& scene_;
    BVH bvh_;

    Vec3 traceRay(const Ray& r) const;
    Vec3 shade(const Ray& r, const Hit& hit) const;
    Vec3 sampleTexture(const Hit& hit) const;
    bool inShadow(const Vec3& p, const Vec3& lightPos) const;
};
