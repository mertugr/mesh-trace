#pragma once

#include "Camera.h"
#include "Image.h"
#include "Light.h"
#include "Material.h"
#include "Mesh.h"
#include "Vec3.h"

#include <string>
#include <vector>

struct Scene {
    int maxRayDepth = 5;
    Vec3 background{0, 0, 0};

    Camera camera;

    AmbientLight ambient;
    std::vector<PointLight> pointLights;
    std::vector<TriangularLight> triangularLights;

    std::vector<Material> materials;

    std::vector<Vec3> vertices;    // 0-based
    std::vector<Vec3> normals;     // 0-based
    std::vector<Vec3> texCoords;   // 0-based; x = u, y = v, z unused

    Image texture;                 // empty if no textureimage provided
    bool hasTexture = false;
    std::string textureImageName;

    std::vector<Triangle> triangles; // flat list across all meshes
    std::vector<Mesh> meshes;

    static Scene load(const std::string& xmlPath);
};
