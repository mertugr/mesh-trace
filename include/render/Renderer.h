#pragma once

#include <cstddef>

#include "io/Image.h"
#include "scene/Scene.h"

namespace rt {

class Renderer {
public:
    explicit Renderer(const Scene& scene);

    Image render() const;

private:
    Vec3 trace(const Ray& ray, int depth) const;

    const Scene& scene_;
    double shadowBias_{1e-4};
    std::size_t threadCount_{1};
};

} // namespace rt
