#include "RayTracer.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

RayTracer::RayTracer(const Scene& scene) : scene_(scene) {
    bvh_.build(scene_.triangles, scene_.vertices);
}

namespace {

Vec3 clamp255(const Vec3& c) {
    return Vec3{std::fmin(std::fmax(c.x, 0.0f), 255.0f),
                std::fmin(std::fmax(c.y, 0.0f), 255.0f),
                std::fmin(std::fmax(c.z, 0.0f), 255.0f)};
}

// Barycentric interpolation of three vectors.
Vec3 bary(const Vec3& a, const Vec3& b, const Vec3& c, float b0, float b1, float b2) {
    return a * b0 + b * b1 + c * b2;
}

} // namespace

bool RayTracer::inShadow(const Vec3& p, const Vec3& lightPos) const {
    Vec3 toLight = lightPos - p;
    float distToLight = toLight.length();
    if (distToLight <= 0.0f) return false;
    Vec3 dir = toLight / distToLight;
    Ray shadowRay(p, dir);
    // tMin is small positive; tMax is just before the light to allow the light itself.
    return bvh_.occluded(shadowRay, scene_.triangles, scene_.vertices,
                         1e-4f, distToLight - 1e-4f);
}

Vec3 RayTracer::sampleTexture(const Hit& hit) const {
    if (!scene_.hasTexture) return Vec3{0, 0, 0};
    const Triangle& tri = scene_.triangles[hit.triangleIdx];
    if (tri.t0 < 0 || tri.t1 < 0 || tri.t2 < 0) return Vec3{0, 0, 0};
    if (tri.t0 >= static_cast<int>(scene_.texCoords.size()) ||
        tri.t1 >= static_cast<int>(scene_.texCoords.size()) ||
        tri.t2 >= static_cast<int>(scene_.texCoords.size())) {
        return Vec3{0, 0, 0};
    }
    const Vec3& a = scene_.texCoords[tri.t0];
    const Vec3& b = scene_.texCoords[tri.t1];
    const Vec3& c = scene_.texCoords[tri.t2];
    float u = a.x * hit.b0 + b.x * hit.b1 + c.x * hit.b2;
    float v = a.y * hit.b0 + b.y * hit.b1 + c.y * hit.b2;
    return scene_.texture.sampleBilinear(u, v);
}

Vec3 RayTracer::shade(const Ray& r, const Hit& hit) const {
    const Material& mat = scene_.materials[hit.materialIdx];

    // Shading normal: interpolated per-vertex if available, else geometric.
    const Triangle& tri = scene_.triangles[hit.triangleIdx];
    Vec3 n = hit.faceNormal;
    if (tri.n0 >= 0 && tri.n1 >= 0 && tri.n2 >= 0 &&
        tri.n0 < static_cast<int>(scene_.normals.size()) &&
        tri.n1 < static_cast<int>(scene_.normals.size()) &&
        tri.n2 < static_cast<int>(scene_.normals.size())) {
        n = bary(scene_.normals[tri.n0], scene_.normals[tri.n1], scene_.normals[tri.n2],
                 hit.b0, hit.b1, hit.b2).normalized();
    }
    // Flip toward the incoming ray so we always shade the front of what we see.
    if (n.dot(r.direction) > 0.0f) n = -n;

    // Offset the surface point along the normal to avoid self-shadowing.
    Vec3 pOffset = hit.point + n * kShadowEpsilon;
    Vec3 viewDir = -r.direction; // already unit

    // Ambient.
    Vec3 color = scene_.ambient.intensity * mat.ambient;

    // Point lights.
    for (const PointLight& L : scene_.pointLights) {
        Vec3 toL = L.position - pOffset;
        float dist = toL.length();
        if (dist <= 0.0f) continue;
        Vec3 lDir = toL / dist;
        if (inShadow(pOffset, L.position)) continue;

        float cosT = std::fmax(0.0f, n.dot(lDir));
        Vec3 irradiance = L.intensity / (dist * dist);

        color += mat.diffuse * irradiance * cosT;

        Vec3 h = (lDir + viewDir).normalized();
        float cosA = std::fmax(0.0f, n.dot(h));
        float sp = std::pow(cosA, mat.phongExponent);
        color += mat.specular * irradiance * sp;
    }

    // Triangular (planar directional) lights.
    for (const TriangularLight& L : scene_.triangularLights) {
        // Emit only toward the direction of the cross product.
        Vec3 toSurf = pOffset - L.centroid;
        if (toSurf.dot(L.direction) <= 0.0f) continue;

        Vec3 toL = L.centroid - pOffset;
        float dist = toL.length();
        if (dist <= 0.0f) continue;
        Vec3 lDir = toL / dist;
        if (inShadow(pOffset, L.centroid)) continue;

        float cosT = std::fmax(0.0f, n.dot(lDir));
        Vec3 irradiance = L.intensity / (dist * dist);

        color += mat.diffuse * irradiance * cosT;

        Vec3 h = (lDir + viewDir).normalized();
        float cosA = std::fmax(0.0f, n.dot(h));
        float sp = std::pow(cosA, mat.phongExponent);
        color += mat.specular * irradiance * sp;
    }

    // Mirror reflection (recursive).
    if (r.depth < scene_.maxRayDepth && mat.isMirror()) {
        Vec3 d = r.direction;
        Vec3 refl = d - n * (2.0f * n.dot(d));
        refl = refl.normalized();
        Ray reflected(pOffset, refl, r.depth + 1);
        Vec3 reflColor = traceRay(reflected);
        color += mat.mirrorReflectance * reflColor;
    }

    // Texture blend.
    if (mat.textureFactor > 0.0f && scene_.hasTexture) {
        Vec3 texColor = sampleTexture(hit);
        float tf = mat.textureFactor;
        color = texColor * tf + color * (1.0f - tf);
    }

    return color;
}

Vec3 RayTracer::traceRay(const Ray& r) const {
    Hit hit;
    if (!bvh_.intersect(r, scene_.triangles, scene_.vertices,
                        std::numeric_limits<float>::infinity(), hit)) {
        // Any ray that leaves the scene samples the background; a mirror that
        // sees the sky shows the sky.
        return scene_.background;
    }
    return shade(r, hit);
}

void RayTracer::render(Image& out, int threadCount) {
    const int W = scene_.camera.imageWidth;
    const int H = scene_.camera.imageHeight;
    out.resize(W, H);

    if (threadCount <= 0) {
        threadCount = static_cast<int>(std::thread::hardware_concurrency());
        if (threadCount <= 0) threadCount = 4;
    }
    threadCount = std::min(threadCount, H);
    std::cout << "Rendering " << W << "x" << H << " with " << threadCount
              << " thread(s), " << scene_.triangles.size() << " triangles, "
              << bvh_.nodeCount() << " BVH nodes\n";

    // Tiles are contiguous row bands assigned via an atomic counter.
    std::atomic<int> nextRow{0};
    const int rowsPerChunk = std::max(1, H / (threadCount * 8));

    auto worker = [&]() {
        while (true) {
            int startRow = nextRow.fetch_add(rowsPerChunk, std::memory_order_relaxed);
            if (startRow >= H) break;
            int endRow = std::min(H, startRow + rowsPerChunk);
            for (int j = startRow; j < endRow; ++j) {
                for (int i = 0; i < W; ++i) {
                    Ray r = scene_.camera.primaryRay(i, j);
                    Vec3 color = traceRay(r);
                    out.setPixel(i, j, clamp255(color));
                }
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(threadCount);
    for (int t = 0; t < threadCount; ++t) threads.emplace_back(worker);
    for (auto& th : threads) th.join();
}
