#pragma once

#include "core/Ray.h"
#include "math/Vec3.h"

namespace rt {

class Camera {
public:
    Vec3 position{0.0, 0.0, 0.0};
    Vec3 gaze{0.0, 0.0, -1.0};
    Vec3 up{0.0, 1.0, 0.0};

    double left{-1.0};
    double right{1.0};
    double bottom{-1.0};
    double top{1.0};
    double nearDistance{1.0};

    int imageWidth{800};
    int imageHeight{800};

    void prepare();
    Ray generateRay(int px, int py) const;

private:
    Vec3 u_;
    Vec3 v_;
    Vec3 w_;
};

} // namespace rt
