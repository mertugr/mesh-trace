#include "core/BVH.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace rt {
namespace {

AABB triangleBounds(const Triangle& tri) {
    AABB box;
    box.expand(tri.v0);
    box.expand(tri.v1);
    box.expand(tri.v2);
    return box;
}

Vec3 triangleCentroid(const Triangle& tri) {
    return Vec3(
        (tri.v0.x + tri.v1.x + tri.v2.x) / 3.0,
        (tri.v0.y + tri.v1.y + tri.v2.y) / 3.0,
        (tri.v0.z + tri.v1.z + tri.v2.z) / 3.0
    );
}

constexpr int kMaxLeafSize = 4;
constexpr int kSAHBins = 12;

} // namespace

void BVH::build(std::vector<Triangle>& triangles) {
    nodes_.clear();
    if (triangles.empty()) {
        return;
    }
    nodes_.reserve(2 * triangles.size());
    buildRecursive(triangles, 0, static_cast<int>(triangles.size()));
}

int BVH::buildRecursive(std::vector<Triangle>& triangles, int start, int count) {
    const int nodeIdx = static_cast<int>(nodes_.size());
    nodes_.push_back(BVHNode{});
    BVHNode& node = nodes_[nodeIdx];

    AABB centroidBounds;
    for (int i = start; i < start + count; ++i) {
        const AABB tb = triangleBounds(triangles[i]);
        node.bounds.expand(tb);
        centroidBounds.expand(triangleCentroid(triangles[i]));
    }

    if (count <= kMaxLeafSize) {
        node.triStart = start;
        node.triCount = count;
        return nodeIdx;
    }

    // SAH split
    int bestAxis = -1;
    int bestBin = -1;
    double bestCost = std::numeric_limits<double>::max();

    for (int axis = 0; axis < 3; ++axis) {
        const double axisMin = (axis == 0) ? centroidBounds.mn.x : (axis == 1) ? centroidBounds.mn.y : centroidBounds.mn.z;
        const double axisMax = (axis == 0) ? centroidBounds.mx.x : (axis == 1) ? centroidBounds.mx.y : centroidBounds.mx.z;
        if (axisMax - axisMin < 1e-12) {
            continue;
        }

        struct Bin {
            AABB bounds;
            int count{0};
        };
        Bin bins[kSAHBins];

        const double scale = static_cast<double>(kSAHBins) / (axisMax - axisMin);
        for (int i = start; i < start + count; ++i) {
            const Vec3 c = triangleCentroid(triangles[i]);
            const double cv = (axis == 0) ? c.x : (axis == 1) ? c.y : c.z;
            int b = static_cast<int>((cv - axisMin) * scale);
            b = std::clamp(b, 0, kSAHBins - 1);
            bins[b].count++;
            bins[b].bounds.expand(triangleBounds(triangles[i]));
        }

        // Sweep from left
        AABB leftBoxes[kSAHBins - 1];
        int leftCounts[kSAHBins - 1];
        {
            AABB running;
            int runCount = 0;
            for (int i = 0; i < kSAHBins - 1; ++i) {
                running.expand(bins[i].bounds);
                runCount += bins[i].count;
                leftBoxes[i] = running;
                leftCounts[i] = runCount;
            }
        }

        // Sweep from right
        {
            AABB running;
            int runCount = 0;
            for (int i = kSAHBins - 1; i >= 1; --i) {
                running.expand(bins[i].bounds);
                runCount += bins[i].count;
                const int leftCount = leftCounts[i - 1];
                if (leftCount == 0 || runCount == 0) {
                    continue;
                }
                const double cost = leftCount * leftBoxes[i - 1].surfaceArea() + runCount * running.surfaceArea();
                if (cost < bestCost) {
                    bestCost = cost;
                    bestAxis = axis;
                    bestBin = i;
                }
            }
        }
    }

    if (bestAxis == -1) {
        // Fallback: split in half along longest axis
        const double dx = centroidBounds.mx.x - centroidBounds.mn.x;
        const double dy = centroidBounds.mx.y - centroidBounds.mn.y;
        const double dz = centroidBounds.mx.z - centroidBounds.mn.z;
        bestAxis = (dx >= dy && dx >= dz) ? 0 : (dy >= dz) ? 1 : 2;

        std::sort(triangles.begin() + start, triangles.begin() + start + count,
            [bestAxis](const Triangle& a, const Triangle& b) {
                const Vec3 ca = triangleCentroid(a);
                const Vec3 cb = triangleCentroid(b);
                return (bestAxis == 0) ? ca.x < cb.x : (bestAxis == 1) ? ca.y < cb.y : ca.z < cb.z;
            });

        const int mid = count / 2;
        node.leftChild = buildRecursive(triangles, start, mid);
        node.rightChild = buildRecursive(triangles, start + mid, count - mid);
        nodes_[nodeIdx].bounds = node.bounds; // re-grab after potential realloc
        return nodeIdx;
    }

    // Partition by SAH split
    const double axisMin = (bestAxis == 0) ? centroidBounds.mn.x : (bestAxis == 1) ? centroidBounds.mn.y : centroidBounds.mn.z;
    const double axisMax = (bestAxis == 0) ? centroidBounds.mx.x : (bestAxis == 1) ? centroidBounds.mx.y : centroidBounds.mx.z;
    const double scale = static_cast<double>(kSAHBins) / (axisMax - axisMin);

    auto mid = std::partition(triangles.begin() + start, triangles.begin() + start + count,
        [bestAxis, axisMin, scale, bestBin](const Triangle& tri) {
            const Vec3 c = triangleCentroid(tri);
            const double cv = (bestAxis == 0) ? c.x : (bestAxis == 1) ? c.y : c.z;
            int b = static_cast<int>((cv - axisMin) * scale);
            b = std::clamp(b, 0, kSAHBins - 1);
            return b < bestBin;
        });

    int leftCount = static_cast<int>(mid - (triangles.begin() + start));
    if (leftCount == 0 || leftCount == count) {
        leftCount = count / 2;
    }

    node.leftChild = buildRecursive(triangles, start, leftCount);
    node.rightChild = buildRecursive(triangles, start + leftCount, count - leftCount);
    // Recompute bounds after potential vector reallocation
    nodes_[nodeIdx].bounds = AABB{};
    nodes_[nodeIdx].bounds.expand(nodes_[nodes_[nodeIdx].leftChild].bounds);
    nodes_[nodeIdx].bounds.expand(nodes_[nodes_[nodeIdx].rightChild].bounds);
    return nodeIdx;
}

bool BVH::intersect(const Ray& ray, const std::vector<Triangle>& triangles, Intersection& closest) const {
    if (nodes_.empty()) {
        return false;
    }

    const Vec3 invDir(
        1.0 / (std::abs(ray.direction.x) > 1e-15 ? ray.direction.x : (ray.direction.x >= 0 ? 1e-15 : -1e-15)),
        1.0 / (std::abs(ray.direction.y) > 1e-15 ? ray.direction.y : (ray.direction.y >= 0 ? 1e-15 : -1e-15)),
        1.0 / (std::abs(ray.direction.z) > 1e-15 ? ray.direction.z : (ray.direction.z >= 0 ? 1e-15 : -1e-15))
    );

    closest.hit = false;
    closest.t = ray.tMax;

    int stack[64];
    int stackTop = 0;
    stack[stackTop++] = 0;

    bool anyHit = false;

    while (stackTop > 0) {
        const int idx = stack[--stackTop];
        const BVHNode& node = nodes_[idx];

        if (!node.bounds.intersect(invDir, ray.origin, ray.tMin, closest.t)) {
            continue;
        }

        if (node.isLeaf()) {
            for (int i = node.triStart; i < node.triStart + node.triCount; ++i) {
                if (triangles[i].intersect(ray, closest)) {
                    anyHit = true;
                }
            }
        } else {
            stack[stackTop++] = node.leftChild;
            stack[stackTop++] = node.rightChild;
        }
    }

    return anyHit;
}

} // namespace rt
