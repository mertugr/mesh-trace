#include "Scene.h"

#include "XmlParser.h"

#include <cctype>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string dirOf(const std::string& path) {
    std::size_t p = path.find_last_of("/\\");
    return p == std::string::npos ? std::string() : path.substr(0, p + 1);
}

Vec3 parseVec3(const std::string& text) {
    std::istringstream iss(text);
    float x = 0, y = 0, z = 0;
    iss >> x >> y >> z;
    return Vec3{x, y, z};
}

void requireChild(const XmlNode& node, const char* name) {
    if (!node.firstChild(name)) {
        throw std::runtime_error("missing <" + std::string(name) + "> under <" + node.name + ">");
    }
}

// Parse a single face-vertex token like "2/1/1", "2//1", "2/1", or "2".
// Returns 0-based indices; -1 indicates "not provided".
struct FaceVert {
    int v = -1, t = -1, n = -1;
};

FaceVert parseFaceVert(const std::string& tok) {
    FaceVert fv;
    std::size_t p1 = tok.find('/');
    if (p1 == std::string::npos) {
        fv.v = std::stoi(tok) - 1;
        return fv;
    }
    if (p1 > 0) fv.v = std::stoi(tok.substr(0, p1)) - 1;
    std::size_t p2 = tok.find('/', p1 + 1);
    if (p2 == std::string::npos) {
        if (p1 + 1 < tok.size()) fv.t = std::stoi(tok.substr(p1 + 1)) - 1;
    } else {
        if (p2 > p1 + 1) fv.t = std::stoi(tok.substr(p1 + 1, p2 - p1 - 1)) - 1;
        if (p2 + 1 < tok.size()) fv.n = std::stoi(tok.substr(p2 + 1)) - 1;
    }
    return fv;
}

int findMaterial(const std::vector<Material>& mats, const std::string& id) {
    for (std::size_t i = 0; i < mats.size(); ++i) {
        if (mats[i].id == id) return static_cast<int>(i);
    }
    // Allow numeric id fallback: if id is a positive integer, use (id-1) directly
    // (matches the common convention in example scenes).
    try {
        int idx = std::stoi(id) - 1;
        if (idx >= 0 && idx < static_cast<int>(mats.size())) return idx;
    } catch (...) {}
    throw std::runtime_error("material id not found: " + id);
}

} // namespace

Scene Scene::load(const std::string& xmlPath) {
    XmlNode root = XmlParser::parseFile(xmlPath);
    if (root.name != "scene") {
        throw std::runtime_error("root element must be <scene>, got <" + root.name + ">");
    }
    std::string sceneDir = dirOf(xmlPath);

    Scene s;

    if (auto n = root.firstChild("maxraytracedepth")) {
        s.maxRayDepth = std::stoi(trim(n->text));
    }

    // Spec uses <background>; the assignment's example XML uses <backgroundColor>. Accept both.
    const XmlNode* bg = root.firstChild("background");
    if (!bg) bg = root.firstChild("backgroundColor");
    if (bg) s.background = parseVec3(bg->text);

    // ---- camera ----
    if (auto c = root.firstChild("camera")) {
        requireChild(*c, "position");
        requireChild(*c, "gaze");
        requireChild(*c, "up");
        s.camera.position = parseVec3(c->firstChild("position")->text);
        s.camera.gaze     = parseVec3(c->firstChild("gaze")->text);
        s.camera.up       = parseVec3(c->firstChild("up")->text);

        const XmlNode* np = c->firstChild("nearplane");
        if (!np) np = c->firstChild("nearPlane");
        if (!np) throw std::runtime_error("camera missing <nearplane>");
        {
            std::istringstream iss(np->text);
            iss >> s.camera.left >> s.camera.right >> s.camera.bottom >> s.camera.top;
        }

        const XmlNode* nd = c->firstChild("neardistance");
        if (!nd) nd = c->firstChild("nearDistance");
        if (!nd) throw std::runtime_error("camera missing <neardistance>");
        s.camera.nearDistance = std::stof(trim(nd->text));

        const XmlNode* ir = c->firstChild("imageresolution");
        if (!ir) ir = c->firstChild("imageResolution");
        if (!ir) throw std::runtime_error("camera missing <imageresolution>");
        {
            std::istringstream iss(ir->text);
            iss >> s.camera.imageWidth >> s.camera.imageHeight;
        }

        s.camera.setup();
    }

    // ---- lights ----
    if (auto L = root.firstChild("lights")) {
        if (auto a = L->firstChild("ambientlight")) {
            s.ambient.intensity = parseVec3(a->text);
        }
        for (auto pl : L->childrenByName("pointlight")) {
            PointLight p;
            p.id = pl->attr("id");
            requireChild(*pl, "position");
            requireChild(*pl, "intensity");
            p.position  = parseVec3(pl->firstChild("position")->text);
            p.intensity = parseVec3(pl->firstChild("intensity")->text);
            s.pointLights.push_back(p);
        }
        for (auto tl : L->childrenByName("triangularlight")) {
            TriangularLight t;
            t.id = tl->attr("id");
            requireChild(*tl, "vertex1");
            requireChild(*tl, "vertex2");
            requireChild(*tl, "vertex3");
            requireChild(*tl, "intensity");
            t.v1 = parseVec3(tl->firstChild("vertex1")->text);
            t.v2 = parseVec3(tl->firstChild("vertex2")->text);
            t.v3 = parseVec3(tl->firstChild("vertex3")->text);
            t.intensity = parseVec3(tl->firstChild("intensity")->text);
            Vec3 dirRaw = (t.v1 - t.v2).cross(t.v1 - t.v3);
            float len = dirRaw.length();
            t.direction = len > 0 ? dirRaw / len : Vec3{0, 1, 0};
            t.area = 0.5f * len;
            t.centroid = (t.v1 + t.v2 + t.v3) * (1.0f / 3.0f);
            s.triangularLights.push_back(t);
        }
    }

    // ---- materials ----
    if (auto M = root.firstChild("materials")) {
        for (auto m : M->childrenByName("material")) {
            Material mat;
            mat.id = m->attr("id");
            if (auto x = m->firstChild("ambient"))           mat.ambient = parseVec3(x->text);
            if (auto x = m->firstChild("diffuse"))           mat.diffuse = parseVec3(x->text);
            if (auto x = m->firstChild("specular"))          mat.specular = parseVec3(x->text);
            if (auto x = m->firstChild("mirrorreflectance")) mat.mirrorReflectance = parseVec3(x->text);
            if (auto x = m->firstChild("phongexponent"))     mat.phongExponent = std::stof(trim(x->text));
            if (auto x = m->firstChild("texturefactor"))     mat.textureFactor = std::stof(trim(x->text));
            s.materials.push_back(mat);
        }
    }

    // ---- vertex / normal / texture data ----
    if (auto vd = root.firstChild("vertexdata")) {
        std::istringstream iss(vd->text);
        float x, y, z;
        while (iss >> x >> y >> z) s.vertices.push_back(Vec3{x, y, z});
    }
    if (auto nd = root.firstChild("normaldata")) {
        std::istringstream iss(nd->text);
        float x, y, z;
        while (iss >> x >> y >> z) s.normals.push_back(Vec3{x, y, z});
    }
    if (auto td = root.firstChild("texturedata")) {
        std::istringstream iss(td->text);
        float a, b;
        while (iss >> a >> b) s.texCoords.push_back(Vec3{a, b, 0});
    }

    // ---- texture image ----
    if (auto ti = root.firstChild("textureimage")) {
        s.textureImageName = trim(ti->text);
        if (!s.textureImageName.empty()) {
            std::string full = sceneDir + s.textureImageName;
            try {
                s.texture = loadPpm(full);
                s.hasTexture = true;
            } catch (const std::exception& e) {
                std::cerr << "warning: failed to load texture '" << full << "': " << e.what() << "\n";
                s.hasTexture = false;
            }
        }
    }

    // ---- objects (meshes) ----
    if (auto O = root.firstChild("objects")) {
        for (auto m : O->childrenByName("mesh")) {
            Mesh mesh;
            mesh.id = m->attr("id");

            std::string matId;
            if (auto mid = m->firstChild("materialid")) matId = trim(mid->text);
            else if (auto mid = m->firstChild("materialId")) matId = trim(mid->text);
            else throw std::runtime_error("mesh missing <materialid>");
            mesh.materialIdx = findMaterial(s.materials, matId);

            const XmlNode* faces = m->firstChild("faces");
            if (!faces) throw std::runtime_error("mesh missing <faces>");

            mesh.firstTriangle = static_cast<int>(s.triangles.size());
            std::istringstream iss(faces->text);
            std::vector<std::string> toks;
            std::string tok;
            while (iss >> tok) toks.push_back(tok);
            if (toks.size() % 3 != 0) {
                throw std::runtime_error("face tokens not a multiple of 3 in mesh " + mesh.id);
            }
            const int nv = static_cast<int>(s.vertices.size());
            const int nn = static_cast<int>(s.normals.size());
            const int nt = static_cast<int>(s.texCoords.size());
            auto checkV = [&](int idx) {
                if (idx < 0 || idx >= nv) {
                    throw std::runtime_error("vertex index " + std::to_string(idx + 1)
                        + " out of range [1, " + std::to_string(nv) + "] in mesh " + mesh.id);
                }
            };
            auto checkN = [&](int idx) {
                if (idx >= 0 && idx >= nn) {
                    throw std::runtime_error("normal index " + std::to_string(idx + 1)
                        + " out of range [1, " + std::to_string(nn) + "] in mesh " + mesh.id);
                }
            };
            auto checkT = [&](int idx) {
                if (idx >= 0 && idx >= nt) {
                    throw std::runtime_error("texture index " + std::to_string(idx + 1)
                        + " out of range [1, " + std::to_string(nt) + "] in mesh " + mesh.id);
                }
            };
            for (std::size_t i = 0; i + 2 < toks.size(); i += 3) {
                FaceVert a = parseFaceVert(toks[i]);
                FaceVert b = parseFaceVert(toks[i + 1]);
                FaceVert c = parseFaceVert(toks[i + 2]);
                checkV(a.v); checkV(b.v); checkV(c.v);
                checkN(a.n); checkN(b.n); checkN(c.n);
                checkT(a.t); checkT(b.t); checkT(c.t);
                Triangle tri;
                tri.v0 = a.v; tri.v1 = b.v; tri.v2 = c.v;
                tri.t0 = a.t; tri.t1 = b.t; tri.t2 = c.t;
                tri.n0 = a.n; tri.n1 = b.n; tri.n2 = c.n;
                tri.materialIdx = mesh.materialIdx;
                const Vec3& p0 = s.vertices[tri.v0];
                const Vec3& p1 = s.vertices[tri.v1];
                const Vec3& p2 = s.vertices[tri.v2];
                tri.faceNormal = (p1 - p0).cross(p2 - p0).normalized();
                s.triangles.push_back(tri);
            }
            mesh.triangleCount = static_cast<int>(s.triangles.size()) - mesh.firstTriangle;
            s.meshes.push_back(mesh);
        }
    }

    return s;
}
