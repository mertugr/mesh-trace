#include "scene/Scene.h"

namespace rt {

void Scene::buildBVH() {
    bvh.build(triangles);
}

bool Scene::intersect(const Ray& ray, Intersection& closest) const {
    return bvh.intersect(ray, triangles, closest);
}

const Material* Scene::findMaterial(int id) const {
    const auto it = materials.find(id);
    if (it == materials.end()) {
        return nullptr;
    }
    return &it->second;
}

} // namespace rt
