#pragma once

#include "AABB.h"
#include "Mesh.h"
#include "Ray.h"
#include "Vec3.h"

#include <vector>

// Bounding Volume Hierarchy over a flat triangle array.
// Build splits by the longest axis of the centroid bbox at the median centroid
// (a good middle ground between quality and build speed).
class BVH {
public:
    void build(const std::vector<Triangle>& tris, const std::vector<Vec3>& verts);

    // Closest-hit query. `hit` is written only when a triangle is found.
    bool intersect(const Ray& r, const std::vector<Triangle>& tris,
                   const std::vector<Vec3>& verts, float tMax, Hit& hit) const;

    // Any-hit query for shadow rays. Returns true if any triangle is intersected
    // with t in (tMin, tMax).
    bool occluded(const Ray& r, const std::vector<Triangle>& tris,
                  const std::vector<Vec3>& verts, float tMin, float tMax) const;

    int nodeCount() const { return static_cast<int>(nodes.size()); }

private:
    struct Node {
        AABB bbox;
        int left = -1;   // child index, or -1 for leaf
        int right = -1;
        int start = 0;   // for leaves: range into ordered triangle indices
        int count = 0;   // 0 for internal nodes
    };

    std::vector<Node> nodes;
    std::vector<int> triIndices; // permutation of triangle indices (leaf lookup)

    int buildRecursive(std::vector<int>& indices, const std::vector<Triangle>& tris,
                       const std::vector<Vec3>& verts, int start, int end);
    static bool intersectTriangle(const Ray& r, const Triangle& tri,
                                  const std::vector<Vec3>& verts,
                                  float& tOut, float& b1Out, float& b2Out);
};
