// Self-contained test suite — no external test framework.
//
// Coverage map:
//   Vec3.h           math ops, dot, cross, normalize, reflect
//   Ray.h            implicit (used throughout)
//   Triangle.cpp     hit/miss, UV interpolation, normal interpolation,
//                    face-normal fallback, closest-hit ordering
//   BVH.cpp          correctness vs brute-force, closest hit
//   Camera.cpp       ray direction signs, unit length
//   Texture.cpp      UV order (u=first), bilinear interpolation, tiling
//   Scene.cpp        findMaterial, Scene::intersect via BVH
//   Light.h          PointLight 1/r² attenuation + direction
//                    TriangularLight direction formula
//   Renderer.cpp     ambient shading, diffuse (lit > dark), specular,
//                    shadow blocking, textureFactor blend,
//                    mirror reflection, depth limit
//   Image.cpp        getPixel/setPixel, savePPM + savePNG round-trip
//   SceneParser.cpp  UV order, multiple meshes/materials, error cases
//
// Build:   make test
// Run:     ./tests/test_runner
// Verbose: ./tests/test_runner 2>&1 | grep -E "FAIL|Results"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "core/BVH.h"
#include "core/Intersection.h"
#include "core/Ray.h"
#include "io/Image.h"
#include "io/SceneParser.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include "render/Renderer.h"
#include "scene/Camera.h"
#include "scene/Light.h"
#include "scene/Material.h"
#include "scene/Scene.h"
#include "scene/Texture.h"
#include "scene/Triangle.h"

namespace {

// ---------------------------------------------------------------------------
// Test harness
// ---------------------------------------------------------------------------
int  g_pass  = 0;
int  g_fail  = 0;
const char* g_group = "";

void check(bool cond, const char* expr, const char* file, int line) {
    if (cond) { ++g_pass; }
    else {
        ++g_fail;
        std::fprintf(stderr, "  FAIL  %s:%d  %s  [%s]\n", file, line, expr, g_group);
    }
}

bool near(double a, double b, double eps = 1e-5) { return std::abs(a - b) <= eps; }

bool nearV3(const rt::Vec3& a, const rt::Vec3& b, double eps = 1e-4) {
    return near(a.x,b.x,eps) && near(a.y,b.y,eps) && near(a.z,b.z,eps);
}
bool nearV2(const rt::Vec2& a, const rt::Vec2& b, double eps = 1e-4) {
    return near(a.x,b.x,eps) && near(a.y,b.y,eps);
}

#define GROUP(name) g_group = (name); std::printf("[%s]\n", g_group)
#define CHECK(e)        check((e), #e, __FILE__, __LINE__)
#define CHECK_NEAR(a,b) check(near((a),(b)), #a " ~= " #b, __FILE__, __LINE__)

int summary() {
    std::printf("\n=== Results: %d passed, %d failed ===\n\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
bool writePPM(const char* path, int w, int h, const uint8_t* rgb) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    std::fwrite(rgb, 1, static_cast<size_t>(w * h * 3), f);
    std::fclose(f);
    return true;
}

// Build a minimal valid Scene programmatically (no XML needed)
rt::Scene makeScene(int w = 4, int h = 4) {
    rt::Scene s;
    s.maxRayTraceDepth = 4;
    s.background = {0, 0, 0};
    s.ambientLight = {0, 0, 0};

    s.camera.position    = {0, 0, 5};
    s.camera.gaze        = {0, 0, -1};
    s.camera.up          = {0, 1,  0};
    s.camera.left        = -1; s.camera.right  =  1;
    s.camera.bottom      = -1; s.camera.top    =  1;
    s.camera.nearDistance = 5;
    s.camera.imageWidth  = w;
    s.camera.imageHeight = h;
    s.camera.prepare();
    return s;
}

// Add a flat quad (two triangles) facing +Z centered at the origin
void addQuad(rt::Scene& s, int matId,
             double z = 0.0,
             rt::Vec2 uv00 = {0,0}, rt::Vec2 uv10 = {1,0},
             rt::Vec2 uv11 = {1,1}, rt::Vec2 uv01 = {0,1}) {
    const double R = 0.8;
    const rt::Vec3 N{0, 0, 1};

    rt::Triangle t0;
    t0.v0={-R,-R,z}; t0.v1={ R,-R,z}; t0.v2={ R, R,z};
    t0.n0=t0.n1=t0.n2=N;
    t0.uv0=uv00; t0.uv1=uv10; t0.uv2=uv11;
    t0.materialId=matId;

    rt::Triangle t1;
    t1.v0={-R,-R,z}; t1.v1={ R, R,z}; t1.v2={-R, R,z};
    t1.n0=t1.n1=t1.n2=N;
    t1.uv0=uv00; t1.uv1=uv11; t1.uv2=uv01;
    t1.materialId=matId;

    s.triangles.push_back(t0);
    s.triangles.push_back(t1);
    s.buildBVH();
}

rt::Material makeMat(int id,
                     rt::Vec3 amb={0,0,0}, rt::Vec3 dif={0,0,0},
                     rt::Vec3 spe={0,0,0}, rt::Vec3 mir={0,0,0},
                     double phong=1, double tf=0) {
    rt::Material m;
    m.id=id; m.ambient=amb; m.diffuse=dif; m.specular=spe;
    m.mirrorReflectance=mir; m.phongExponent=phong; m.textureFactor=tf;
    return m;
}

// Render a 4×4 scene and return the center-pixel color (row 1–2, col 1–2 avg)
rt::Vec3 centerPixel(rt::Scene& scene) {
    const rt::Renderer renderer(scene);
    const rt::Image img = renderer.render();
    // Average of the four center pixels of a 4×4 image
    const rt::Vec3 p00 = img.getPixel(1, 1);
    const rt::Vec3 p10 = img.getPixel(2, 1);
    const rt::Vec3 p01 = img.getPixel(1, 2);
    const rt::Vec3 p11 = img.getPixel(2, 2);
    return (p00 + p10 + p01 + p11) * 0.25;
}

// ===========================================================================
// 1. Vec3 math
// ===========================================================================
void test_vec3() {
    GROUP("Vec3 math");

    const rt::Vec3 a{1,2,3}, b{4,5,6};
    CHECK(nearV3(a+b, {5,7,9}));
    CHECK(nearV3(a-b, {-3,-3,-3}));
    CHECK(nearV3(a*2.0, {2,4,6}));
    CHECK_NEAR(rt::Vec3::dot(a,b), 32.0);
    CHECK(nearV3(rt::Vec3::cross(a,b), {-3,6,-3}));
    CHECK(nearV3(rt::Vec3(3,0,0).normalized(), {1,0,0}));
    CHECK_NEAR(rt::Vec3(1,2,3).normalized().length(), 1.0);
    // zero vector → no crash, no NaN
    CHECK(nearV3(rt::Vec3(0,0,0).normalized(), {0,0,0}));
    // reflect: -Z into +Z normal → +Z
    CHECK(nearV3(rt::reflect({0,0,-1},{0,0,1}), {0,0,1}));
    // 45-degree incidence
    CHECK(nearV3(rt::reflect(rt::Vec3(1,-1,0).normalized(),{0,1,0}).normalized(),
                 rt::Vec3(1,1,0).normalized()));
}

// ===========================================================================
// 2. Ray-triangle intersection
// ===========================================================================
void test_triangle_intersection() {
    GROUP("Triangle intersection");

    rt::Triangle tri;
    tri.v0={0,0,0}; tri.v1={1,0,0}; tri.v2={0,1,0};
    tri.n0=tri.n1=tri.n2={0,0,1};
    tri.uv0={0,0}; tri.uv1={1,0}; tri.uv2={0,1};
    tri.materialId=1;

    auto hit = [&](rt::Vec3 o, rt::Vec3 d) {
        rt::Ray r(o, d.normalized());
        rt::Intersection i; i.t = r.tMax;
        return tri.intersect(r, i);
    };

    CHECK( hit({0.25,0.25,1},{0,0,-1}));   // inside triangle
    CHECK(!hit({2.0, 2.0, 1},{0,0,-1}));   // outside
    CHECK(!hit({0.25,0.25,-1},{0,0,-1}));  // behind origin
    CHECK(!hit({0,0,0},{1,0,0}));          // parallel to plane

    // position and t are correct
    {
        rt::Ray r({0.25,0.25,1},{0,0,-1});
        rt::Intersection i; i.t = r.tMax;
        tri.intersect(r, i);
        CHECK_NEAR(i.t, 1.0);
        CHECK(nearV3(i.position, {0.25,0.25,0.0}));
    }

    // BVH returns CLOSEST when two triangles overlap in screen space
    {
        rt::Triangle far = tri;
        far.v0.z=far.v1.z=far.v2.z=-5;
        std::vector<rt::Triangle> both = {far, tri};
        rt::BVH bvh; bvh.build(both);
        rt::Ray r({0.25,0.25,2},{0,0,-1});
        rt::Intersection i; i.t = r.tMax;
        bvh.intersect(r, both, i);
        CHECK_NEAR(i.t, 2.0); // nearer triangle at z=0
    }
}

// ===========================================================================
// 3. Triangle — normal interpolation and face-normal fallback
// ===========================================================================
void test_triangle_normals() {
    GROUP("Triangle normal interpolation");

    // Smooth normals: v0 points +Z, v2 points +X
    rt::Triangle tri;
    tri.v0={0,0,0}; tri.v1={2,0,0}; tri.v2={0,2,0};
    tri.n0={0,0,1}; tri.n1={0,0,1}; tri.n2={1,0,0}; // different normals
    tri.uv0=tri.uv1=tri.uv2={0,0};
    tri.materialId=1;

    // Hit at v0 corner → should get n0 = (0,0,1)
    {
        rt::Ray r({0.01,0.01,1},{0,0,-1});
        rt::Intersection i; i.t = r.tMax;
        tri.intersect(r, i);
        CHECK(i.normal.z > 0.9);  // close to (0,0,1)
        CHECK(i.normal.x < 0.1);
    }

    // Hit at v2 corner → should get n2 ≈ (1,0,0)
    {
        rt::Ray r({0.01,1.98,1},{0,0,-1});
        rt::Intersection i; i.t = r.tMax;
        tri.intersect(r, i);
        CHECK(i.normal.x > 0.9);  // close to (1,0,0)
        CHECK(i.normal.z < 0.1);
    }

    // Face-normal fallback: all normals are zero → auto-compute from vertices
    {
        rt::Triangle t2;
        t2.v0={0,0,0}; t2.v1={1,0,0}; t2.v2={0,1,0};
        t2.n0={0,0,0}; t2.n1={0,0,0}; t2.n2={0,0,0};
        t2.uv0=t2.uv1=t2.uv2={0,0};
        t2.materialId=1;
        rt::Ray r({0.25,0.25,1},{0,0,-1});
        rt::Intersection i; i.t = r.tMax;
        t2.intersect(r, i);
        // face normal for CCW XY triangle = (0,0,1)
        CHECK(nearV3(i.normal, {0,0,1}));
    }
}

// ===========================================================================
// 4. UV interpolation
// ===========================================================================
void test_triangle_uv() {
    GROUP("Triangle UV interpolation");

    rt::Triangle tri;
    tri.v0={0,0,0}; tri.v1={2,0,0}; tri.v2={0,2,0};
    tri.n0=tri.n1=tri.n2={0,0,1};
    tri.uv0={0.1,0.2}; tri.uv1={0.9,0.2}; tri.uv2={0.1,0.8};
    tri.materialId=1;

    auto uv = [&](double x, double y) -> rt::Vec2 {
        rt::Ray r({x,y,1},{0,0,-1}); rt::Intersection i; i.t=r.tMax;
        tri.intersect(r,i); return i.uv;
    };

    CHECK(nearV2(uv(0.01,0.01), {0.1,0.2}, 0.02)); // near v0
    CHECK(nearV2(uv(1.98,0.01), {0.9,0.2}, 0.02)); // near v1
    CHECK(nearV2(uv(0.01,1.98), {0.1,0.8}, 0.02)); // near v2

    // u increases rightward, v stays constant → changing x changes uv.x
    const rt::Vec2 left  = uv(0.2, 0.3);
    const rt::Vec2 right = uv(1.0, 0.3);
    CHECK(left.x < right.x);
    CHECK(near(left.y, right.y, 0.05));
}

// ===========================================================================
// 5. Texture — UV order and bilinear interpolation
// ===========================================================================
void test_texture_uv_order() {
    GROUP("Texture UV order (u=first, v=second)");

    // 2×1: [RED | BLUE] — only u varies
    // uv(0.9, 0) → u=0.9 → near right → mostly BLUE
    // If first were v not u: would stay RED
    {
        const uint8_t pix[6] = {255,0,0, 0,0,255};
        const char* p = "/tmp/rt_2x1.ppm";
        if (writePPM(p,2,1,pix)) {
            rt::ImageTexture tex; CHECK(tex.load(p));
            const rt::Vec3 r = tex.sample({0.9,0.0}) * 255.0;
            CHECK(r.z > r.x); // blue > red → u controls horizontal
            const rt::Vec3 l = tex.sample({0.0,0.0}) * 255.0;
            CHECK(near(l.x, 255.0, 2.0)); // left is RED
            std::remove(p);
        }
    }

    // 1×2: [RED(top) / BLUE(bottom)] — only v varies
    // v=0 → bottom → BLUE,  v=0.9 → near top → mostly RED
    // If second were u not v: uv(0, 0.9) would be the same as uv(0,0)=BLUE
    {
        const uint8_t pix[6] = {255,0,0, 0,0,255}; // row0=RED, row1=BLUE
        const char* p = "/tmp/rt_1x2.ppm";
        if (writePPM(p,1,2,pix)) {
            rt::ImageTexture tex; CHECK(tex.load(p));
            CHECK(nearV3(tex.sample({0.0,0.0})*255.0, {0,0,255}, 2.0)); // v=0→bottom→BLUE
            const rt::Vec3 r = tex.sample({0.0,0.9}) * 255.0;
            CHECK(r.x > r.z); // v=0.9→near top→mostly RED
            std::remove(p);
        }
    }
}

void test_texture_bilinear() {
    GROUP("Texture bilinear interpolation (2x2)");

    // row0=[RED,GREEN], row1=[BLUE,YELLOW]
    const uint8_t pix[12] = {255,0,0, 0,255,0, 0,0,255, 255,255,0};
    const char* p = "/tmp/rt_2x2.ppm";
    if (!writePPM(p,2,2,pix)) { std::printf("  SKIP\n"); return; }

    rt::ImageTexture tex; CHECK(tex.load(p));
    auto s = [&](double u, double v){ return tex.sample({u,v})*255.0; };

    // uv(0,0) → v=0→bottom(row1), u=0→left → BLUE
    CHECK(nearV3(s(0,0), {0,0,255}, 2.0));

    // tiling: uv(1,0) == uv(0,0) because fract(1.0)=0
    CHECK(nearV3(s(1,0), s(0,0), 2.0));

    // center (0.5,0.5): all four pixels blend equally
    // c0=lerp(RED,GREEN,0.5)=(0.5,0.5,0), c1=lerp(BLUE,YELLOW,0.5)=(0.5,0.5,0.5)
    // result=lerp(c0,c1,0.5)=(0.5,0.5,0.25) → *255=(127.5,127.5,63.75)
    const rt::Vec3 c = s(0.5,0.5);
    CHECK(near(c.x,127.5,2.0));
    CHECK(near(c.y,127.5,2.0));
    CHECK(near(c.z, 63.75,2.0));

    // left column at v=0.5: lerp(BLUE,RED,0.5) = (127.5,0,127.5)
    const rt::Vec3 lm = s(0.0,0.5);
    CHECK(near(lm.x,127.5,2.0));
    CHECK(near(lm.y,  0.0,2.0));
    CHECK(near(lm.z,127.5,2.0));

    std::remove(p);
}

// ===========================================================================
// 6. Camera ray generation
// ===========================================================================
void test_camera() {
    GROUP("Camera ray generation");

    rt::Camera cam;
    cam.position={0,0,0}; cam.gaze={0,0,-1}; cam.up={0,1,0};
    cam.left=-1; cam.right=1; cam.bottom=-1; cam.top=1;
    cam.nearDistance=1; cam.imageWidth=100; cam.imageHeight=100;
    cam.prepare();

    for (int px : {0,25,49,74,99}) {
        for (int py : {0,25,49,74,99}) {
            const rt::Ray r = cam.generateRay(px,py);
            CHECK(nearV3(r.origin, {0,0,0}));
            CHECK_NEAR(r.direction.length(), 1.0);
        }
    }
    CHECK(cam.generateRay(49,49).direction.z < 0);            // points -Z
    CHECK(cam.generateRay( 0,50).direction.x < 0);            // left pixels go left
    CHECK(cam.generateRay(99,50).direction.x > 0);            // right pixels go right
    CHECK(cam.generateRay(50, 0).direction.y > 0);            // top pixels go up
    CHECK(cam.generateRay(50,99).direction.y < 0);            // bottom pixels go down
}

// ===========================================================================
// 7. BVH correctness vs brute-force
// ===========================================================================
void test_bvh() {
    GROUP("BVH correctness");

    std::vector<rt::Triangle> tris(5);
    for (int i = 0; i < 5; ++i) {
        const double z = -(i+1.0);
        tris[i].v0={-1,-1,z}; tris[i].v1={1,-1,z}; tris[i].v2={0,1,z};
        tris[i].n0=tris[i].n1=tris[i].n2={0,0,1};
        tris[i].uv0=tris[i].uv1=tris[i].uv2={0,0};
        tris[i].materialId=i+1;
    }
    rt::BVH bvh; bvh.build(tris);

    // Ray hits closest (z=-1, t=1)
    { rt::Ray r({0,0,0},{0,0,-1}); rt::Intersection i; i.t=r.tMax;
      CHECK(bvh.intersect(r,tris,i)); CHECK_NEAR(i.t,1.0); }

    // Miss
    { rt::Ray r({10,10,0},{0,0,-1}); rt::Intersection i; i.t=r.tMax;
      CHECK(!bvh.intersect(r,tris,i)); }

    // BVH == brute force for every combination
    for (int xi=-2; xi<=2; ++xi) for (int yi=-2; yi<=2; ++yi) {
        const double ox=xi*0.2, oy=yi*0.2;
        rt::Ray r({ox,oy,1},{0,0,-1});
        rt::Intersection bf; bf.t=r.tMax;
        for (auto& t:tris) t.intersect(r,bf);
        rt::Intersection bv; bv.t=r.tMax;
        bvh.intersect(r,tris,bv);
        CHECK(bf.hit==bv.hit);
        if (bf.hit) CHECK_NEAR(bf.t,bv.t);
    }
}

// ===========================================================================
// 8. PointLight — 1/r² attenuation and direction
// ===========================================================================
void test_point_light() {
    GROUP("PointLight sampling");

    const rt::PointLight light({0,0,5}, {100,100,100});

    // Sample from surface point directly below the light
    rt::Vec3 L, rad; double maxDist;
    CHECK(light.sample({0,0,0}, L, maxDist, rad));

    // Direction must point toward the light (positive Z)
    CHECK_NEAR(L.x, 0.0);
    CHECK_NEAR(L.y, 0.0);
    CHECK(L.z > 0.0);
    CHECK_NEAR(L.length(), 1.0); // must be normalised

    // maxDistance must equal the actual distance (5 units)
    CHECK_NEAR(maxDist, 5.0);

    // Radiance = intensity / dist² = 100/25 = 4
    CHECK_NEAR(rad.x, 4.0);
    CHECK_NEAR(rad.y, 4.0);
    CHECK_NEAR(rad.z, 4.0);

    // Inverse-square law: doubling distance → quarter radiance
    rt::Vec3 L2, rad2; double dist2;
    light.sample({0,0,-5}, L2, dist2, rad2); // dist = 10
    CHECK_NEAR(dist2, 10.0);
    CHECK_NEAR(rad2.x, 1.0); // 100/100 = 1.0

    // Light exactly at surface point → sample returns false
    rt::Vec3 Lx, rx; double dx;
    CHECK(!light.sample({0,0,5}, Lx, dx, rx));
}

// ===========================================================================
// 9. TriangularLight — direction formula matches spec
// ===========================================================================
void test_triangular_light() {
    GROUP("TriangularLight direction formula");

    // Spec: direction = (vertex1-vertex2) × (vertex1-vertex3)
    // Vertices chosen so direction_ = -Z  (light emits toward -Z)
    // v1=(0,0,0), v2=(0,1,0), v3=(1,0,0)
    // (v1-v2)=(0,-1,0), (v1-v3)=(-1,0,0)
    // cross = ((-1)*0-0*0, 0*(-1)-0*0, 0*0-(-1)*(-1)) = (0,0,-1) → direction_=-Z
    // L = -direction_ = +Z (from surface toward the light)
    const rt::TriangularLight light({0,0,0},{0,1,0},{1,0,0},{50,50,50});

    rt::Vec3 L, rad; double maxDist;
    CHECK(light.sample({0,0,-5}, L, maxDist, rad));

    // L must be the direction from surface toward the light source: +Z
    CHECK(L.z > 0.0);
    CHECK_NEAR(L.length(), 1.0);

    // Radiance equals intensity (no distance attenuation for directional light)
    CHECK_NEAR(rad.x, 50.0);
    CHECK_NEAR(rad.y, 50.0);
    CHECK_NEAR(rad.z, 50.0);

    // maxDistance must be infinite (directional light has no source position)
    CHECK(std::isinf(maxDist) || maxDist > 1e29);

    // Degenerate triangle (all same point) → sample returns false
    const rt::TriangularLight degenerate({1,1,1},{1,1,1},{1,1,1},{10,10,10});
    rt::Vec3 Ld, rd; double dd;
    CHECK(!degenerate.sample({0,0,0}, Ld, dd, rd));
}

// ===========================================================================
// 10. Scene::findMaterial
// ===========================================================================
void test_find_material() {
    GROUP("Scene::findMaterial");

    rt::Scene scene;
    scene.materials[1] = makeMat(1, {1,0,0});
    scene.materials[3] = makeMat(3, {0,1,0});

    CHECK(scene.findMaterial(1) != nullptr);
    CHECK(scene.findMaterial(3) != nullptr);
    CHECK(scene.findMaterial(2) == nullptr); // does not exist
    CHECK(scene.findMaterial(0) == nullptr);

    CHECK(nearV3(scene.findMaterial(1)->ambient, {1,0,0}));
    CHECK(nearV3(scene.findMaterial(3)->ambient, {0,1,0}));
}

// ===========================================================================
// 11. Renderer — ambient shading
//     A scene with only ambient light and no point/triangular lights must
//     produce exactly:  pixel = material.ambient * scene.ambientLight
// ===========================================================================
void test_render_ambient() {
    GROUP("Renderer: ambient shading");

    rt::Scene scene = makeScene();
    scene.ambientLight = {100, 80, 60};
    scene.materials[1] = makeMat(1, {1,1,1}/*amb*/, {1,1,1}/*dif*/);

    addQuad(scene, 1, 0.0);

    const rt::Vec3 c = centerPixel(scene);

    // With no point lights in the scene, only ambient contributes
    // pixel = ambient * ambientLight = (1,1,1) * (100,80,60) = (100,80,60)
    CHECK_NEAR(c.x, 100.0);
    CHECK_NEAR(c.y,  80.0);
    CHECK_NEAR(c.z,  60.0);
}

// ===========================================================================
// 12. Renderer — diffuse lighting (lit side brighter than ambient only)
// ===========================================================================
void test_render_diffuse() {
    GROUP("Renderer: diffuse lighting");

    rt::Scene scene = makeScene();
    scene.ambientLight = {10, 10, 10};
    scene.materials[1] = makeMat(1, {0.1,0.1,0.1}/*amb*/, {1,1,1}/*dif*/);

    // Point light directly in front of the quad (along +Z)
    scene.lights.push_back(std::make_unique<rt::PointLight>(
        rt::Vec3{0,0,10}, rt::Vec3{1000,1000,1000}));

    addQuad(scene, 1, 0.0);

    const rt::Vec3 c = centerPixel(scene);

    // Must be brighter than ambient-only result (1,1,1)
    CHECK(c.x > 1.0);
    CHECK(c.y > 1.0);
    CHECK(c.z > 1.0);

    // Diffuse must be symmetric: left ≈ right, top ≈ bottom (light is centred)
    const rt::Renderer renderer(scene);
    const rt::Image img = renderer.render();
    CHECK(near(img.getPixel(1,1).x, img.getPixel(2,1).x, 5.0));
    CHECK(near(img.getPixel(1,1).y, img.getPixel(1,2).y, 5.0));
}

// ===========================================================================
// 13. Renderer — specular highlight
//     Specular term = (V·R)^phong. With a high phong exponent, the highlight
//     concentrates near the mirror-reflection direction.
//     We verify that the lit surface is brighter when specular is on than off.
// ===========================================================================
void test_render_specular() {
    GROUP("Renderer: specular highlight");

    auto makeSpec = [](rt::Vec3 spe, double phong) {
        rt::Scene scene = makeScene();
        scene.ambientLight = {5,5,5};
        scene.materials[1] = makeMat(1, {0,0,0}, {0.5,0.5,0.5}, spe, {0,0,0}, phong);
        scene.lights.push_back(std::make_unique<rt::PointLight>(
            rt::Vec3{0,0,10}, rt::Vec3{500,500,500}));
        addQuad(scene, 1, 0.0);
        return centerPixel(scene);
    };

    const rt::Vec3 noSpec  = makeSpec({0,0,0}, 32);
    const rt::Vec3 withSpec = makeSpec({1,1,1}, 32);

    // Adding specular should only increase brightness
    CHECK(withSpec.x >= noSpec.x);
    CHECK(withSpec.y >= noSpec.y);
    CHECK(withSpec.z >= noSpec.z);
}

// ===========================================================================
// 14. Renderer — shadow blocking
//     A blocker placed between the surface and the light makes the surface
//     darker (ambient only) compared to the unblocked case.
// ===========================================================================
void test_render_shadow() {
    GROUP("Renderer: shadow blocking");

    // Unblocked scene
    rt::Scene unblocked = makeScene();
    unblocked.ambientLight = {10,10,10};
    unblocked.materials[1] = makeMat(1, {0.1,0.1,0.1}, {1,1,1});
    unblocked.lights.push_back(std::make_unique<rt::PointLight>(
        rt::Vec3{0,0,10}, rt::Vec3{500,500,500}));
    addQuad(unblocked, 1, 0.0);
    const rt::Vec3 lit = centerPixel(unblocked);

    // Blocked scene: add an opaque quad between light and surface
    rt::Scene blocked = makeScene();
    blocked.ambientLight = {10,10,10};
    blocked.materials[1] = makeMat(1, {0.1,0.1,0.1}, {1,1,1});
    blocked.materials[2] = makeMat(2, {0,0,0}, {0,0,0}); // opaque blocker
    blocked.lights.push_back(std::make_unique<rt::PointLight>(
        rt::Vec3{0,0,10}, rt::Vec3{500,500,500}));
    // Surface at z=0
    {
        rt::Triangle t;
        t.v0={-0.8,-0.8,0}; t.v1={0.8,-0.8,0}; t.v2={0.8,0.8,0};
        t.n0=t.n1=t.n2={0,0,1}; t.uv0=t.uv1=t.uv2={0,0}; t.materialId=1;
        blocked.triangles.push_back(t);
    }
    {
        rt::Triangle t;
        t.v0={-0.8,-0.8,0}; t.v1={0.8,0.8,0}; t.v2={-0.8,0.8,0};
        t.n0=t.n1=t.n2={0,0,1}; t.uv0=t.uv1=t.uv2={0,0}; t.materialId=1;
        blocked.triangles.push_back(t);
    }
    // Blocker at z=3 (between surface at z=0 and light at z=10)
    {
        rt::Triangle t;
        t.v0={-0.8,-0.8,3}; t.v1={0.8,-0.8,3}; t.v2={0.8,0.8,3};
        t.n0=t.n1=t.n2={0,0,-1}; t.uv0=t.uv1=t.uv2={0,0}; t.materialId=2;
        blocked.triangles.push_back(t);
    }
    {
        rt::Triangle t;
        t.v0={-0.8,-0.8,3}; t.v1={0.8,0.8,3}; t.v2={-0.8,0.8,3};
        t.n0=t.n1=t.n2={0,0,-1}; t.uv0=t.uv1=t.uv2={0,0}; t.materialId=2;
        blocked.triangles.push_back(t);
    }
    blocked.buildBVH();
    const rt::Vec3 dark = centerPixel(blocked);

    // Blocked surface must be dimmer than unblocked
    CHECK(dark.x < lit.x);
    CHECK(dark.y < lit.y);
    CHECK(dark.z < lit.z);
}

// ===========================================================================
// 15. Renderer — textureFactor blending
//     textureFactor=0  → pure shaded color (no texture)
//     textureFactor=1  → pure texture color (ignores shading)
//     textureFactor=0.5 → midpoint
// ===========================================================================
void test_render_texture_factor() {
    GROUP("Renderer: textureFactor blending");

    // Solid red texture (1×1)
    const uint8_t red[3] = {255, 0, 0};
    const char* texPath = "/tmp/rt_red.ppm";
    if (!writePPM(texPath, 1, 1, red)) { std::printf("  SKIP\n"); return; }

    auto makeBlend = [&](double tf) {
        rt::Scene scene = makeScene();
        scene.ambientLight = {100, 100, 100};           // white ambient
        scene.materials[1] = makeMat(1,
            {1,1,1},{0,0,0},{0,0,0},{0,0,0}, 1, tf);   // white material
        scene.texture = std::make_shared<rt::ImageTexture>();
        scene.texture->load(texPath);
        addQuad(scene, 1);
        return centerPixel(scene);
    };

    const rt::Vec3 tf0  = makeBlend(0.0); // pure shading: white ambient on white mat
    const rt::Vec3 tf1  = makeBlend(1.0); // pure texture: red (×255)
    const rt::Vec3 tf05 = makeBlend(0.5); // half blend

    // tf=0 → shaded color, no texture influence
    //   shaded = ambient(1,1,1)*ambientLight(100,100,100) = (100,100,100)
    CHECK_NEAR(tf0.x, 100.0);
    CHECK_NEAR(tf0.y, 100.0);
    CHECK_NEAR(tf0.z, 100.0);

    // tf=1 → pure texture: red=(255,0,0)
    CHECK_NEAR(tf1.x, 255.0);
    CHECK_NEAR(tf1.y,   0.0);
    CHECK_NEAR(tf1.z,   0.0);

    // tf=0.5 → blend: shaded*(0.5)+texture*(0.5)
    //   = (100,100,100)*0.5 + (255,0,0)*0.5 = (177.5, 50, 50)
    CHECK_NEAR(tf05.x, 177.5);
    CHECK_NEAR(tf05.y,  50.0);
    CHECK_NEAR(tf05.z,  50.0);

    std::remove(texPath);
}

// ===========================================================================
// 16. Renderer — mirror reflection
//     A mirror material must pick up color from reflected geometry.
//     Without geometry behind the mirror, it reflects the background.
// ===========================================================================
void test_render_mirror() {
    GROUP("Renderer: mirror reflection");

    // Scene: mirror quad facing camera, background = red
    // The mirror should reflect the background → mirror pixel ≈ background
    rt::Scene scene = makeScene();
    scene.background = {200, 0, 0};
    scene.ambientLight = {0, 0, 0};
    scene.materials[1] = makeMat(1, {0,0,0},{0,0,0},{0,0,0},{1,1,1}); // pure mirror
    addQuad(scene, 1, 0.0);

    const rt::Vec3 c = centerPixel(scene);

    // Mirror with no other geometry reflects the background
    CHECK_NEAR(c.x, 200.0);
    CHECK_NEAR(c.y,   0.0);
    CHECK_NEAR(c.z,   0.0);
}

// ===========================================================================
// 17. Renderer — depth limit (maxRayTraceDepth)
//     At depth 0, mirror recursion is not allowed → falls back to background
// ===========================================================================
void test_render_depth_limit() {
    GROUP("Renderer: maxRayTraceDepth");

    // depth=0 means no mirror recursion at all
    rt::Scene scene = makeScene();
    scene.maxRayTraceDepth = 0;
    scene.background = {50, 50, 50};
    scene.ambientLight = {0, 0, 0};
    scene.materials[1] = makeMat(1, {0,0,0},{0,0,0},{0,0,0},{1,1,1}); // pure mirror
    addQuad(scene, 1, 0.0);

    const rt::Vec3 c = centerPixel(scene);

    // At depth=0 the mirror can still compute ambient (=0), but cannot recurse
    // The reflected ray is not traced → mirrors show (0,0,0) or just ambient
    // Key assertion: no infinite recursion and result is finite
    CHECK(!std::isnan(c.x));
    CHECK(!std::isnan(c.y));
    CHECK(!std::isnan(c.z));

    // Background color: a ray that misses all geometry returns background
    rt::Scene emptyScene = makeScene();
    emptyScene.background = {77, 88, 99};
    emptyScene.materials[1] = makeMat(1);
    // Add one triangle far off to the side so the centre hits nothing
    rt::Triangle t;
    t.v0={10,10,0}; t.v1={11,10,0}; t.v2={10,11,0};
    t.n0=t.n1=t.n2={0,0,1}; t.uv0=t.uv1=t.uv2={0,0}; t.materialId=1;
    emptyScene.triangles.push_back(t);
    emptyScene.buildBVH();
    const rt::Renderer r2(emptyScene);
    const rt::Image img2 = r2.render();
    // Centre pixels (1,1) and (2,2) should be background
    CHECK_NEAR(img2.getPixel(1,1).x, 77.0);
    CHECK_NEAR(img2.getPixel(1,1).y, 88.0);
    CHECK_NEAR(img2.getPixel(1,1).z, 99.0);
}

// ===========================================================================
// 18. Image — getPixel/setPixel and PPM/PNG round-trip
// ===========================================================================
void test_image_io() {
    GROUP("Image I/O (set/get pixel, PPM, PNG)");

    rt::Image img(4, 4);
    img.setPixel(0, 0, {255,   0,   0});
    img.setPixel(3, 3, {  0, 255,   0});
    img.setPixel(2, 1, {  0,   0, 255});

    CHECK(nearV3(img.getPixel(0,0), {255,  0,  0}));
    CHECK(nearV3(img.getPixel(3,3), {  0,255,  0}));
    CHECK(nearV3(img.getPixel(2,1), {  0,  0,255}));
    CHECK(nearV3(img.getPixel(1,1), {  0,  0,  0})); // untouched = black

    // Out-of-bounds returns black (no crash)
    CHECK(nearV3(img.getPixel(-1,  0), {0,0,0}));
    CHECK(nearV3(img.getPixel(  0,-1), {0,0,0}));
    CHECK(nearV3(img.getPixel(100,  0), {0,0,0}));

    // PPM round-trip
    {
        const char* path = "/tmp/rt_img_test.ppm";
        CHECK(img.savePPM(path));
        std::ifstream f(path, std::ios::binary);
        CHECK(f.good());
        std::string magic; f >> magic;
        CHECK(magic == "P6");
        std::remove(path);
    }

    // PNG round-trip
    {
        const char* path = "/tmp/rt_img_test.png";
        CHECK(img.savePNG(path));
        std::ifstream f(path, std::ios::binary);
        CHECK(f.good());
        // PNG signature: first 8 bytes are 137 80 78 71 13 10 26 10
        char sig[8];
        f.read(sig, 8);
        CHECK(static_cast<unsigned char>(sig[0]) == 137);
        CHECK(sig[1]=='P' && sig[2]=='N' && sig[3]=='G');
        std::remove(path);
    }
}

// ===========================================================================
// 19. SceneParser — multiple meshes and materials, error cases
// ===========================================================================
void test_parser_multiple_meshes() {
    GROUP("SceneParser: multiple meshes and materials");

    const char* xml = R"(
<scene>
  <maxraytracedepth>2</maxraytracedepth>
  <background>5 10 15</background>
  <camera>
    <position>0 0 5</position><gaze>0 0 -1</gaze><up>0 1 0</up>
    <nearplane>-1 1 -1 1</nearplane><neardistance>1</neardistance>
    <imageresolution>8 8</imageresolution>
  </camera>
  <lights><ambientlight>20 20 20</ambientlight></lights>
  <materials>
    <material id="1">
      <ambient>1 0 0</ambient><diffuse>1 0 0</diffuse>
      <specular>0 0 0</specular><mirrorreflectance>0 0 0</mirrorreflectance>
      <phongexponent>1</phongexponent><texturefactor>0</texturefactor>
    </material>
    <material id="2">
      <ambient>0 1 0</ambient><diffuse>0 1 0</diffuse>
      <specular>0 0 0</specular><mirrorreflectance>0 0 0</mirrorreflectance>
      <phongexponent>1</phongexponent><texturefactor>0</texturefactor>
    </material>
  </materials>
  <vertexdata>
    -1 -1 0   1 -1 0   0  1 0
     2 -1 0   4 -1 0   3  1 0
  </vertexdata>
  <objects>
    <mesh id="1"><materialid>1</materialid>
      <faces>1/0/0 2/0/0 3/0/0</faces></mesh>
    <mesh id="2"><materialid>2</materialid>
      <faces>4/0/0 5/0/0 6/0/0</faces></mesh>
  </objects>
</scene>)";

    const char* path = "/tmp/rt_multi.xml";
    { FILE* f=std::fopen(path,"w"); std::fputs(xml,f); std::fclose(f); }

    rt::Scene scene; rt::SceneParser parser; std::string err;
    CHECK(parser.parse(path, scene, err));

    CHECK(scene.triangles.size() == 2);
    CHECK(scene.materials.size() == 2);
    CHECK(scene.triangles[0].materialId == 1);
    CHECK(scene.triangles[1].materialId == 2);
    CHECK_NEAR(scene.background.x, 5.0);
    CHECK_NEAR(scene.maxRayTraceDepth, 2);

    std::remove(path);
}

void test_parser_errors() {
    GROUP("SceneParser: error cases");

    rt::SceneParser parser;

    auto fails = [&](const char* xml) {
        const char* path = "/tmp/rt_err.xml";
        FILE* f=std::fopen(path,"w"); std::fputs(xml,f); std::fclose(f);
        rt::Scene s; std::string e;
        bool ok = parser.parse(path, s, e);
        std::remove(path);
        return !ok;
    };

    // Missing <scene> root
    CHECK(fails("<notscene></notscene>"));

    // Missing <camera>
    CHECK(fails(R"(<scene>
      <maxraytracedepth>1</maxraytracedepth>
      <background>0 0 0</background>
    </scene>)"));

    // Non-existent file
    { rt::Scene s; std::string e;
      CHECK(!parser.parse("/tmp/does_not_exist_xyz.xml", s, e)); }
}

// ===========================================================================
// 20. SceneParser — UV order (u first, v second)
// ===========================================================================
void test_parser_uv_order() {
    GROUP("SceneParser: texturedata u,v order");

    const char* xml = R"(
<scene>
  <maxraytracedepth>1</maxraytracedepth>
  <background>0 0 0</background>
  <camera>
    <position>0 0 5</position><gaze>0 0 -1</gaze><up>0 1 0</up>
    <nearplane>-1 1 -1 1</nearplane><neardistance>1</neardistance>
    <imageresolution>4 4</imageresolution>
  </camera>
  <lights><ambientlight>255 255 255</ambientlight></lights>
  <materials>
    <material id="1">
      <ambient>1 1 1</ambient><diffuse>1 1 1</diffuse>
      <specular>0 0 0</specular><mirrorreflectance>0 0 0</mirrorreflectance>
      <phongexponent>1</phongexponent><texturefactor>0</texturefactor>
    </material>
  </materials>
  <vertexdata>0 0 0   1 0 0   0 1 0</vertexdata>
  <texturedata>0.3 0.7</texturedata>
  <objects>
    <mesh id="1"><materialid>1</materialid>
      <faces>1/1/0 2/1/0 3/1/0</faces></mesh>
  </objects>
</scene>)";

    const char* path = "/tmp/rt_uv.xml";
    { FILE* f=std::fopen(path,"w"); std::fputs(xml,f); std::fclose(f); }

    rt::Scene scene; rt::SceneParser parser; std::string err;
    CHECK(parser.parse(path, scene, err));
    CHECK(!scene.triangles.empty());

    // "0.3 0.7" → u=0.3 (uv.x), v=0.7 (uv.y)
    CHECK_NEAR(scene.triangles[0].uv0.x, 0.3);
    CHECK_NEAR(scene.triangles[0].uv0.y, 0.7);

    std::remove(path);
}

// ===========================================================================
// 21. Shadow bias — no self-intersection, blocker detected
// ===========================================================================
void test_shadow_bias() {
    GROUP("Shadow bias");

    rt::Triangle tri;
    tri.v0={-5,-5,0}; tri.v1={5,-5,0}; tri.v2={0,5,0};
    tri.n0=tri.n1=tri.n2={0,0,1};
    tri.uv0=tri.uv1=tri.uv2={0,0};
    tri.materialId=1;

    rt::Ray primary({0.2,0.3,2},{0,0,-1});
    rt::Intersection hit; hit.t=primary.tMax;
    CHECK(tri.intersect(primary, hit));

    const double bias = 1e-4;
    rt::Vec3 shadowOrig = hit.position + hit.normal.normalized() * bias;
    rt::Ray shadowRay(shadowOrig, {0,0,1}, bias, 1e30);

    // Same triangle must NOT self-intersect
    rt::Intersection self; self.t=shadowRay.tMax;
    CHECK(!tri.intersect(shadowRay, self));

    // Blocker at z=1 MUST be found
    rt::Triangle blocker;
    blocker.v0={-5,-5,1}; blocker.v1={5,-5,1}; blocker.v2={0,5,1};
    blocker.n0=blocker.n1=blocker.n2={0,0,-1};
    blocker.uv0=blocker.uv1=blocker.uv2={0,0};
    blocker.materialId=2;

    rt::Intersection bl; bl.t=shadowRay.tMax;
    CHECK(blocker.intersect(shadowRay, bl));
    CHECK_NEAR(bl.t, 1.0-bias);
}

} // namespace

int main() {
    std::printf("\n=== Ray Tracer Unit Tests ===\n\n");

    test_vec3();
    test_triangle_intersection();
    test_triangle_normals();
    test_triangle_uv();
    test_texture_uv_order();
    test_texture_bilinear();
    test_camera();
    test_bvh();
    test_point_light();
    test_triangular_light();
    test_find_material();
    test_render_ambient();
    test_render_diffuse();
    test_render_specular();
    test_render_shadow();
    test_render_texture_factor();
    test_render_mirror();
    test_render_depth_limit();
    test_image_io();
    test_parser_multiple_meshes();
    test_parser_errors();
    test_parser_uv_order();
    test_shadow_bias();

    return summary();
}
