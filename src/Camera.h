#pragma once

#include "Ray.h"
#include "Vec3.h"

struct Camera {
    Vec3 position{0, 0, 0};
    Vec3 gaze{0, 0, -1};
    Vec3 up{0, 1, 0};
    float left = -1, right = 1, bottom = -1, top = 1;
    float nearDistance = 1.0f;
    int imageWidth = 800;
    int imageHeight = 800;

    // Orthonormal camera basis (computed by setup()).
    // w points away from the scene, u to the right, v up.
    Vec3 u{1, 0, 0};
    Vec3 v{0, 1, 0};
    Vec3 w{0, 0, 1};

    void setup() {
        Vec3 gazeN = gaze.normalized();
        w = -gazeN;
        Vec3 upxw = up.cross(w);
        if (upxw.lengthSquared() < 1e-10f) {
            // up is (nearly) parallel to gaze — pick a fallback reference
            // axis that is guaranteed not to be parallel to w.
            Vec3 alt = std::fabs(w.x) < 0.9f ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
            upxw = alt.cross(w);
        }
        u = upxw.normalized();
        v = w.cross(u); // already unit since u, w are orthonormal
    }

    // (i, j) pixel with j=0 at the top row of the image.
    Ray primaryRay(int i, int j) const {
        float su = left + (right - left) * (i + 0.5f) / imageWidth;
        float sv = top - (top - bottom) * (j + 0.5f) / imageHeight;
        // Image plane point: e - d*w + su*u + sv*v
        Vec3 s = position - w * nearDistance + u * su + v * sv;
        Vec3 d = (s - position).normalized();
        return Ray(position, d, 0);
    }
};
