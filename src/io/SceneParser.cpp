#include "io/SceneParser.h"

#include <filesystem>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

#include "tinyxml2.h"

namespace rt {
namespace {

using tinyxml2::XMLElement;

bool parseVec3(const std::string& text, Vec3& out) {
    std::istringstream ss(text);
    ss >> out.x >> out.y >> out.z;
    return ss.fail() == false;
}

std::vector<double> parseDoubles(const std::string& text) {
    std::istringstream ss(text);
    std::vector<double> vals;
    double v = 0.0;
    while (ss >> v) {
        vals.push_back(v);
    }
    return vals;
}

const char* childText(const XMLElement* parent, const char* name) {
    const XMLElement* c = parent ? parent->FirstChildElement(name) : nullptr;
    return c ? c->GetText() : nullptr;
}

struct FaceRef {
    int v{0};
    int t{0};
    int n{0};
};

bool parseFaceRef(const std::string& token, FaceRef& ref) {
    std::stringstream ss(token);
    std::string a;
    std::string b;
    std::string c;

    if (static_cast<bool>(std::getline(ss, a, '/')) == false) {
        return false;
    }
    std::getline(ss, b, '/');
    std::getline(ss, c, '/');

    if (a.empty()) {
        return false;
    }

    ref.v = std::stoi(a);
    ref.t = b.empty() ? 0 : std::stoi(b);
    ref.n = c.empty() ? 0 : std::stoi(c);
    return true;
}

bool inRangeIndex(int idx, size_t n) {
    return idx > 0 && static_cast<size_t>(idx) <= n;
}

} // namespace

bool SceneParser::parse(const std::string& filePath, Scene& scene, std::string& errorMessage) const {
    try {
        scene = Scene{};

        tinyxml2::XMLDocument doc;
        if (doc.LoadFile(filePath.c_str()) != tinyxml2::XML_SUCCESS) {
            errorMessage = "Failed to load XML file";
            return false;
        }

        const XMLElement* root = doc.FirstChildElement("scene");
        if (root == nullptr) {
            errorMessage = "Missing <scene> root";
            return false;
        }

    if (const char* text = childText(root, "maxraytracedepth")) {
        scene.maxRayTraceDepth = std::stoi(text);
        if (scene.maxRayTraceDepth < 0) {
            errorMessage = "maxraytracedepth must be non-negative";
            return false;
        }
    }
        if (const char* text = childText(root, "background")) {
            parseVec3(text, scene.background);
        } else if (const char* text = childText(root, "backgroundColor")) {
            parseVec3(text, scene.background);
        }

        const XMLElement* camera = root->FirstChildElement("camera");
        if (camera == nullptr) {
            errorMessage = "Missing <camera>";
            return false;
        }

    {
        const char* text = childText(camera, "position");
        if (text == nullptr || parseVec3(text, scene.camera.position) == false) {
            errorMessage = "Invalid camera position";
            return false;
        }
    }
    {
        const char* text = childText(camera, "gaze");
        if (text == nullptr || parseVec3(text, scene.camera.gaze) == false) {
            errorMessage = "Invalid camera gaze";
            return false;
        }
    }
    {
        const char* text = childText(camera, "up");
        if (text == nullptr || parseVec3(text, scene.camera.up) == false) {
            errorMessage = "Invalid camera up";
            return false;
        }
    }
    {
        const char* text = childText(camera, "nearplane");
        if (text == nullptr) {
            text = childText(camera, "nearPlane");
        }
        if (text == nullptr) {
            errorMessage = "Invalid camera nearplane";
            return false;
        }
        const std::vector<double> np = parseDoubles(text);
        if (np.size() != 4) {
            errorMessage = "nearplane must have 4 values";
            return false;
        }
        scene.camera.left = np[0];
        scene.camera.right = np[1];
        scene.camera.bottom = np[2];
        scene.camera.top = np[3];
    }
    {
        const char* text = childText(camera, "neardistance");
        if (text == nullptr) {
            errorMessage = "Invalid camera neardistance";
            return false;
        }
        scene.camera.nearDistance = std::stod(text);
    }
    {
        const char* text = childText(camera, "imageresolution");
        if (text == nullptr) {
            errorMessage = "Invalid image resolution";
            return false;
        }
        std::istringstream ss(text);
        ss >> scene.camera.imageWidth >> scene.camera.imageHeight;
        if (ss.fail() || scene.camera.imageWidth <= 0 || scene.camera.imageHeight <= 0) {
            errorMessage = "Invalid image resolution values";
            return false;
        }
    }
    scene.camera.prepare();

    const XMLElement* lights = root->FirstChildElement("lights");
    if (lights) {
        if (const char* text = childText(lights, "ambientlight")) {
            parseVec3(text, scene.ambientLight);
        }

        for (const XMLElement* l = lights->FirstChildElement(); l; l = l->NextSiblingElement()) {
            const std::string name = l->Name();
            if (name == "pointlight") {
                Vec3 pos;
                Vec3 inten;
                const char* p = childText(l, "position");
                const char* i = childText(l, "intensity");
                if (p == nullptr || i == nullptr || parseVec3(p, pos) == false || parseVec3(i, inten) == false) {
                    errorMessage = "Invalid pointlight entry";
                    return false;
                }
                scene.lights.push_back(std::make_unique<PointLight>(pos, inten));
            } else if (name == "triangularlight") {
                Vec3 v0;
                Vec3 v1;
                Vec3 v2;
                Vec3 inten;
                const char* a = childText(l, "vertex1");
                const char* b = childText(l, "vertex2");
                const char* c = childText(l, "vertex3");
                const char* i = childText(l, "intensity");
                if (a == nullptr || b == nullptr || c == nullptr || i == nullptr ||
                    parseVec3(a, v0) == false || parseVec3(b, v1) == false || parseVec3(c, v2) == false || parseVec3(i, inten) == false) {
                    errorMessage = "Invalid triangularlight entry";
                    return false;
                }
                scene.lights.push_back(std::make_unique<TriangularLight>(v0, v1, v2, inten));
            }
        }
    }

    const XMLElement* materials = root->FirstChildElement("materials");
    if (materials) {
        for (const XMLElement* m = materials->FirstChildElement("material"); m; m = m->NextSiblingElement("material")) {
            Material mat;
            m->QueryIntAttribute("id", &mat.id);
            if (mat.id <= 0) {
                errorMessage = "Material id must be positive";
                return false;
            }

            const char* amb = childText(m, "ambient");
            const char* dif = childText(m, "diffuse");
            const char* spe = childText(m, "specular");
            const char* mir = childText(m, "mirrorreflectance");
            const char* pho = childText(m, "phongexponent");

            if (amb == nullptr || dif == nullptr || spe == nullptr || mir == nullptr || pho == nullptr) {
                errorMessage = "Material is missing fields";
                return false;
            }

            if (parseVec3(amb, mat.ambient) == false || parseVec3(dif, mat.diffuse) == false ||
                parseVec3(spe, mat.specular) == false || parseVec3(mir, mat.mirrorReflectance) == false) {
                errorMessage = "Material vector parse failure";
                return false;
            }

            mat.phongExponent = std::stod(pho);
            if (const char* tf = childText(m, "texturefactor")) {
                mat.textureFactor = std::stod(tf);
            }
            scene.materials[mat.id] = mat;
        }
    }

    std::vector<Vec3> vertices;
    std::vector<Vec3> normals;
    std::vector<Vec2> texcoords;

    if (const char* text = childText(root, "vertexdata")) {
        const std::vector<double> vals = parseDoubles(text);
        if (vals.size() % 3 != 0) {
            errorMessage = "vertexdata must have multiples of 3";
            return false;
        }
        for (size_t i = 0; i < vals.size(); i += 3) {
            vertices.push_back({vals[i], vals[i + 1], vals[i + 2]});
        }
    }

    if (const char* text = childText(root, "normaldata")) {
        const std::vector<double> vals = parseDoubles(text);
        if (vals.size() % 3 != 0) {
            errorMessage = "normaldata must have multiples of 3";
            return false;
        }
        for (size_t i = 0; i < vals.size(); i += 3) {
            normals.push_back({vals[i], vals[i + 1], vals[i + 2]});
        }
    }

    if (const char* text = childText(root, "texturedata")) {
        const std::vector<double> vals = parseDoubles(text);
        if (vals.size() % 2 != 0) {
            errorMessage = "texturedata must have multiples of 2";
            return false;
        }
        for (size_t i = 0; i < vals.size(); i += 2) {
            texcoords.push_back({vals[i], vals[i + 1]});
        }
    }

    if (vertices.empty()) {
        errorMessage = "No vertexdata found";
        return false;
    }

    if (const char* textureName = childText(root, "textureimage")) {
        scene.texture = std::make_shared<ImageTexture>();
        std::filesystem::path texPath(textureName);
        if (texPath.is_absolute() == false) {
            texPath = std::filesystem::path(filePath).parent_path() / texPath;
        }
        if (scene.texture->load(texPath.string()) == false) {
            errorMessage = "Failed to load texture image";
            return false;
        }
    }

    const XMLElement* objects = root->FirstChildElement("objects");
    if (objects == nullptr) {
        errorMessage = "Missing <objects>";
        return false;
    }

    for (const XMLElement* obj = objects->FirstChildElement(); obj; obj = obj->NextSiblingElement()) {
        const std::string objectName = obj->Name();
        if (objectName != "mesh") {
            errorMessage = "Only triangle meshes are supported";
            return false;
        }

        const char* matText = childText(obj, "materialid");
        if (matText == nullptr) {
            errorMessage = "Mesh missing materialid";
            return false;
        }
        const int materialId = std::stoi(matText);
        if (scene.findMaterial(materialId) == nullptr) {
            errorMessage = "Mesh materialid does not exist";
            return false;
        }

        const XMLElement* facesElem = obj->FirstChildElement("faces");
        if (facesElem == nullptr || facesElem->GetText() == nullptr) {
            errorMessage = "Mesh missing faces data";
            return false;
        }

        std::vector<FaceRef> refs;
        std::istringstream ss(facesElem->GetText());
        std::string token;
        while (ss >> token) {
            FaceRef fr;
            if (parseFaceRef(token, fr) == false) {
                errorMessage = "Invalid face token: " + token;
                return false;
            }
            refs.push_back(fr);
        }

        if (refs.size() % 3 != 0) {
            errorMessage = "Faces data must define triangles";
            return false;
        }

        for (size_t i = 0; i < refs.size(); i += 3) {
            const FaceRef a = refs[i];
            const FaceRef b = refs[i + 1];
            const FaceRef c = refs[i + 2];

            if (inRangeIndex(a.v, vertices.size()) == false ||
                inRangeIndex(b.v, vertices.size()) == false ||
                inRangeIndex(c.v, vertices.size()) == false) {
                errorMessage = "Vertex index out of range";
                return false;
            }

            Triangle tri;
            tri.materialId = materialId;
            tri.v0 = vertices[static_cast<size_t>(a.v - 1)];
            tri.v1 = vertices[static_cast<size_t>(b.v - 1)];
            tri.v2 = vertices[static_cast<size_t>(c.v - 1)];

            if (inRangeIndex(a.n, normals.size()) && inRangeIndex(b.n, normals.size()) && inRangeIndex(c.n, normals.size())) {
                tri.n0 = normals[static_cast<size_t>(a.n - 1)];
                tri.n1 = normals[static_cast<size_t>(b.n - 1)];
                tri.n2 = normals[static_cast<size_t>(c.n - 1)];
            } else {
                const Vec3 faceNormal = Vec3::cross(tri.v1 - tri.v0, tri.v2 - tri.v0).normalized();
                tri.n0 = faceNormal;
                tri.n1 = faceNormal;
                tri.n2 = faceNormal;
            }

            if (inRangeIndex(a.t, texcoords.size()) && inRangeIndex(b.t, texcoords.size()) && inRangeIndex(c.t, texcoords.size())) {
                tri.uv0 = texcoords[static_cast<size_t>(a.t - 1)];
                tri.uv1 = texcoords[static_cast<size_t>(b.t - 1)];
                tri.uv2 = texcoords[static_cast<size_t>(c.t - 1)];
            } else {
                tri.uv0 = {0.0, 0.0};
                tri.uv1 = {0.0, 0.0};
                tri.uv2 = {0.0, 0.0};
            }

            scene.triangles.push_back(tri);
        }
    }

        if (scene.triangles.empty()) {
            errorMessage = "No triangles in scene";
            return false;
        }

        scene.buildBVH();

        return true;
    } catch (const std::exception& e) {
        errorMessage = std::string("Scene parse exception: ") + e.what();
        return false;
    } catch (...) {
        errorMessage = "Scene parse exception: unknown";
        return false;
    }
}

} // namespace rt
