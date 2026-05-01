#include "BVH.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr int kLeafSize = 4;

AABB triangleBounds(const Triangle& t, const std::vector<Vec3>& verts) {
    AABB b;
    b.expand(verts[t.v0]);
    b.expand(verts[t.v1]);
    b.expand(verts[t.v2]);
    return b;
}

} // namespace

bool BVH::intersectTriangle(const Ray& r, const Triangle& tri,
                            const std::vector<Vec3>& verts,
                            float& tOut, float& b1Out, float& b2Out) {
    // Möller-Trumbore using precomputed edges (set by Scene::load).
    const Vec3& p0 = verts[tri.v0];
    const Vec3& e1 = tri.e1;
    const Vec3& e2 = tri.e2;
    Vec3 pvec = r.direction.cross(e2);
    float det = e1.dot(pvec);
    if (std::fabs(det) < 1e-8f) return false;
    float invDet = 1.0f / det;
    Vec3 tvec = r.origin - p0;
    float u = tvec.dot(pvec) * invDet;
    if (u < 0.0f || u > 1.0f) return false;
    Vec3 qvec = tvec.cross(e1);
    float v = r.direction.dot(qvec) * invDet;
    if (v < 0.0f || u + v > 1.0f) return false;
    float t = e2.dot(qvec) * invDet;
    if (t <= 0.0f) return false;
    tOut = t;
    b1Out = u;
    b2Out = v;
    return true;
}

void BVH::build(const std::vector<Triangle>& tris, const std::vector<Vec3>& verts) {
    nodes.clear();
    triIndices.clear();
    if (tris.empty()) return;
    triIndices.resize(tris.size());
    for (std::size_t i = 0; i < tris.size(); ++i) triIndices[i] = static_cast<int>(i);
    nodes.reserve(tris.size() * 2);

    // Precompute centroids once; the recursive splitter reads them by index
    // instead of recomputing per comparison.
    std::vector<Vec3> centroids(tris.size());
    for (std::size_t i = 0; i < tris.size(); ++i) {
        const Triangle& t = tris[i];
        centroids[i] = (verts[t.v0] + verts[t.v1] + verts[t.v2]) * (1.0f / 3.0f);
    }

    buildRecursive(triIndices, tris, verts, centroids, 0, static_cast<int>(tris.size()));
}

int BVH::buildRecursive(std::vector<int>& indices, const std::vector<Triangle>& tris,
                        const std::vector<Vec3>& verts,
                        const std::vector<Vec3>& centroids, int start, int end) {
    int nodeIdx = static_cast<int>(nodes.size());
    nodes.push_back({});
    Node& node = nodes[nodeIdx];

    AABB bbox;
    AABB centroidBounds;
    for (int i = start; i < end; ++i) {
        const Triangle& t = tris[indices[i]];
        bbox.expand(triangleBounds(t, verts));
        centroidBounds.expand(centroids[indices[i]]);
    }
    node.bbox = bbox;

    int count = end - start;
    if (count <= kLeafSize) {
        node.start = start;
        node.count = count;
        return nodeIdx;
    }

    int axis = centroidBounds.longestAxis();
    // Degenerate centroid span -> leaf.
    if (centroidBounds.bmax[axis] - centroidBounds.bmin[axis] <= 0.0f) {
        node.start = start;
        node.count = count;
        return nodeIdx;
    }

    int mid = start + count / 2;
    // nth_element sorts by centroid along chosen axis using the cached array.
    std::nth_element(indices.begin() + start, indices.begin() + mid, indices.begin() + end,
                     [&](int a, int b) {
                         return centroids[a][axis] < centroids[b][axis];
                     });

    int leftIdx = buildRecursive(indices, tris, verts, centroids, start, mid);
    int rightIdx = buildRecursive(indices, tris, verts, centroids, mid, end);
    // NB: pushing children invalidates `node` reference, so re-access by index.
    nodes[nodeIdx].left = leftIdx;
    nodes[nodeIdx].right = rightIdx;
    nodes[nodeIdx].count = 0;
    return nodeIdx;
}

namespace {
// Reciprocal that replaces exact zeros with a large value of the correct sign,
// so (bmin - origin) * invDir never evaluates to 0 * inf = NaN in the slab test.
inline float safeInv(float d) {
    if (d == 0.0f) return 1e30f;
    return 1.0f / d;
}
} // namespace

bool BVH::intersect(const Ray& r, const std::vector<Triangle>& tris,
                    const std::vector<Vec3>& verts, float tMax, Hit& hit) const {
    if (nodes.empty()) return false;

    Vec3 invDir{safeInv(r.direction.x), safeInv(r.direction.y), safeInv(r.direction.z)};

    int stack[256];
    int top = 0;
    stack[top++] = 0;

    bool found = false;
    float bestT = tMax;
    float bestB1 = 0, bestB2 = 0;
    int bestTriIdx = -1;

    while (top > 0) {
        int ni = stack[--top];
        const Node& n = nodes[ni];
        if (!n.bbox.intersect(r, invDir, 1e-4f, bestT)) continue;

        if (n.count > 0) {
            // leaf
            for (int k = 0; k < n.count; ++k) {
                int triIdx = triIndices[n.start + k];
                const Triangle& tri = tris[triIdx];
                float tHit, b1, b2;
                if (intersectTriangle(r, tri, verts, tHit, b1, b2)) {
                    if (tHit < bestT) {
                        bestT = tHit;
                        bestB1 = b1;
                        bestB2 = b2;
                        bestTriIdx = triIdx;
                        found = true;
                    }
                }
            }
        } else {
            // Push the farther child first so the nearer is popped next.
            const Node& lc = nodes[n.left];
            const Node& rc = nodes[n.right];
            // Approximate near/far by bbox centers along ray.
            float dl = (lc.bbox.centroid() - r.origin).dot(r.direction);
            float dr = (rc.bbox.centroid() - r.origin).dot(r.direction);
            if (dl < dr) {
                stack[top++] = n.right;
                stack[top++] = n.left;
            } else {
                stack[top++] = n.left;
                stack[top++] = n.right;
            }
        }
    }

    if (found) {
        hit.t = bestT;
        hit.b1 = bestB1;
        hit.b2 = bestB2;
        hit.b0 = 1.0f - bestB1 - bestB2;
        hit.triangleIdx = bestTriIdx;
        hit.point = r.at(bestT);
        const Triangle& tri = tris[bestTriIdx];
        hit.materialIdx = tri.materialIdx;
        hit.faceNormal = tri.faceNormal;
    }
    return found;
}

bool BVH::occluded(const Ray& r, const std::vector<Triangle>& tris,
                   const std::vector<Vec3>& verts, float tMin, float tMax) const {
    if (nodes.empty()) return false;

    Vec3 invDir{safeInv(r.direction.x), safeInv(r.direction.y), safeInv(r.direction.z)};

    int stack[256];
    int top = 0;
    stack[top++] = 0;

    while (top > 0) {
        int ni = stack[--top];
        const Node& n = nodes[ni];
        if (!n.bbox.intersect(r, invDir, tMin, tMax)) continue;

        if (n.count > 0) {
            for (int k = 0; k < n.count; ++k) {
                int triIdx = triIndices[n.start + k];
                const Triangle& tri = tris[triIdx];
                float tHit, b1, b2;
                if (intersectTriangle(r, tri, verts, tHit, b1, b2)) {
                    if (tHit > tMin && tHit < tMax) return true;
                }
            }
        } else {
            stack[top++] = n.left;
            stack[top++] = n.right;
        }
    }
    return false;
}
