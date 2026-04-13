#include "render/Renderer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>
#include <vector>

namespace rt {

Renderer::Renderer(const Scene& scene) : scene_(scene) {
    const unsigned int hw = std::thread::hardware_concurrency();
    threadCount_ = static_cast<std::size_t>(hw == 0 ? 1 : hw);
}

Image Renderer::render() const {
    Image img(scene_.camera.imageWidth, scene_.camera.imageHeight);

    const int height = scene_.camera.imageHeight;
    const int width = scene_.camera.imageWidth;
    const std::size_t workers = std::max<std::size_t>(1, std::min<std::size_t>(threadCount_, static_cast<std::size_t>(height)));

    std::vector<std::thread> threads;
    threads.reserve(workers);

    const int rowsPerWorker = std::max(1, height / static_cast<int>(workers));
    for (std::size_t w = 0; w < workers; ++w) {
        const int yBegin = static_cast<int>(w) * rowsPerWorker;
        const int yEnd = (w + 1 == workers) ? height : std::min(height, yBegin + rowsPerWorker);

        threads.emplace_back([this, &img, width, yBegin, yEnd]() {
            for (int y = yBegin; y < yEnd; ++y) {
                for (int x = 0; x < width; ++x) {
                    const Ray ray = scene_.camera.generateRay(x, y);
                    const Vec3 color = trace(ray, 0);
                    img.setPixel(x, y, color);
                }
            }
        });
    }

    for (std::thread& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    return img;
}

Vec3 Renderer::trace(const Ray& ray, int depth) const {
    if (depth > scene_.maxRayTraceDepth) {
        return scene_.background;
    }

    Intersection hit;
    hit.t = ray.tMax;
    if (scene_.intersect(ray, hit) == false) {
        return scene_.background;
    }

    const Material* material = scene_.findMaterial(hit.triangle->materialId);
    if (material == nullptr) {
        return scene_.background;
    }

    Vec3 N = hit.normal.normalized();
    if (Vec3::dot(N, ray.direction) > 0.0) {
        N = N * -1.0;
    }

    const bool hasTexture = (scene_.texture != nullptr) && (scene_.texture->empty() == false);
    const Vec3 textureColor = hasTexture ? (scene_.texture->sample(hit.uv) * 255.0) : Vec3(0.0, 0.0, 0.0);
    const double textureFactor = std::clamp(material->textureFactor, 0.0, 1.0);

    Vec3 shadedColor = material->ambient * scene_.ambientLight;

    const Vec3 viewDir = (ray.direction * -1.0).normalized();

    for (const auto& light : scene_.lights) {
        Vec3 L;
        double maxDistance = 0.0;
        Vec3 lightRadiance;
        if (!light->sample(hit.position, L, maxDistance, lightRadiance)) {
            continue;
        }

        const double shadowMax = std::isfinite(maxDistance) ? std::max(shadowBias_, maxDistance - shadowBias_) : std::numeric_limits<double>::max();
        Ray shadowRay(hit.position + N * shadowBias_, L, shadowBias_, shadowMax);
        Intersection shadowHit;
        shadowHit.t = shadowRay.tMax;
        if (scene_.intersect(shadowRay, shadowHit)) {
            continue;
        }

        const double ndotl = std::max(0.0, Vec3::dot(N, L));
        const Vec3 diffuse = material->diffuse * ndotl;

        const Vec3 R = reflect(L * -1.0, N).normalized();
        const double specTerm = std::pow(std::max(0.0, Vec3::dot(viewDir, R)), material->phongExponent);
        const Vec3 specular = material->specular * specTerm;

        shadedColor += (diffuse + specular) * lightRadiance;
    }

    const bool hasMirror = (material->mirrorReflectance.x > 0.0) || (material->mirrorReflectance.y > 0.0) || (material->mirrorReflectance.z > 0.0);
    if (hasMirror && depth < scene_.maxRayTraceDepth) {
        const Vec3 reflDir = reflect(ray.direction.normalized(), N).normalized();
        const Ray reflRay(hit.position + N * shadowBias_, reflDir);
        const Vec3 reflColor = trace(reflRay, depth + 1);
        shadedColor += material->mirrorReflectance * reflColor;
    }

    if (hasTexture) {
        return shadedColor * (1.0 - textureFactor) + textureColor * textureFactor;
    }
    return shadedColor;
}

} // namespace rt
