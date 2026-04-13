#pragma once

#include <string>

#include "scene/Scene.h"

namespace rt {

class SceneParser {
public:
    bool parse(const std::string& filePath, Scene& scene, std::string& errorMessage) const;
};

} // namespace rt
