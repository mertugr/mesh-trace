#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "core/BVH.h"
#include "core/Intersection.h"
#include "core/Ray.h"
#include "math/Vec3.h"
#include "scene/Camera.h"
#include "scene/Light.h"
#include "scene/Material.h"
#include "scene/Texture.h"
#include "scene/Triangle.h"

namespace rt {

class Scene {
public:
    int maxRayTraceDepth{3};
    Vec3 background{0.0, 0.0, 0.0};
    Vec3 ambientLight{0.0, 0.0, 0.0};

    Camera camera;
    std::vector<std::unique_ptr<Light>> lights;
    std::unordered_map<int, Material> materials;
    std::vector<Triangle> triangles;
    std::shared_ptr<ImageTexture> texture;
    BVH bvh;

    void buildBVH();
    bool intersect(const Ray& ray, Intersection& closest) const;
    const Material* findMaterial(int id) const;
};

} // namespace rt
