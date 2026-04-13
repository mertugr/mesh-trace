# Ray Tracer

A CPU ray tracer written in C++17. Reads an XML scene description, renders it using recursive Whitted-style ray tracing, and writes the result as a PNG or PPM image.

---

## Features

### Core (Required)
- Triangle-only geometry — mesh faces defined as `vertexIdx/texIdx/normalIdx` (OBJ-style)
- Ray-triangle intersection via Möller-Trumbore algorithm
- Phong illumination model — ambient + diffuse + specular
- Shadow rays with configurable epsilon bias to avoid self-intersection
- Mirror reflections — recursive ray tracing up to `maxraytracedepth`
- Texture mapping with per-material `texturefactor` blending
  - `1.0` → pure texture color, shading has no effect
  - `0.0` → shading only, texture has no effect
  - `0.0–1.0` → linear blend between shaded color and texture
- Point lights — radiance falls off with inverse square of distance
- Triangular directional lights — direction = `(v1−v2) × (v1−v3)`
- Multiple materials, lights, and meshes per scene

### Optimization
- **BVH with SAH** — Surface Area Heuristic bounding volume hierarchy; O(log n) ray-scene intersection instead of O(n). Built once after parsing, traversed with an iterative stack during rendering.
- **Multithreading** — Image rows distributed across all available CPU cores via `std::thread`.

### Libraries Used
| Library | Purpose |
|---|---|
| `tinyxml2` | XML scene file parsing |
| `stb_image.h` | Texture loading — PNG, JPG, BMP, PPM, TGA, etc. |
| `stb_image_write.h` | PNG output |

---

## Build

### Make (recommended)
```bash
make -j8
```

### CMake
```bash
cmake -S . -B build
cmake --build build -j8
```

No external dependencies — zlib and other system libraries are **not** required.

---

## Run

```bash
./raytracer <scene.xml> [output.ppm|output.png]
```

- If the output argument is omitted, it defaults to `output.ppm`
- The output format is determined by the file extension (`.png` or `.ppm`)

### Examples
```bash
# Render to PNG
./raytracer scene_full_test.xml output.png

# Render to PPM
./raytracer scene_full_test.xml output.ppm

# UV/texture debug scene
./raytracer scene_uv_debug.xml output_uv.png
```

### Open the result on macOS
```bash
open output.png
```

---

## Scene Files

| File | Description |
|---|---|
| `scene_full_test.xml` | Full feature scene — 4 meshes, 3 lights, textures, mirror, multiple materials |
| `scene_uv_debug.xml` | UV coordinate and texture blending verification scene |

---

## Project Structure

```
cgodev/
  include/
    core/         Ray.h, Intersection.h, BVH.h
    math/         Vec3.h, Vec2.h
    scene/        Camera.h, Light.h, Material.h, Scene.h, Texture.h, Triangle.h
    io/           Image.h, SceneParser.h
    render/       Renderer.h
  src/
    core/         BVH.cpp
    scene/        Camera.cpp, Scene.cpp, Texture.cpp, Triangle.cpp
    io/           Image.cpp, SceneParser.cpp
    render/       Renderer.cpp
    main.cpp
  stb_image.h           Texture loading
  stb_image_write.h     PNG output
  tinyxml2.h / .cpp     XML parsing
  Makefile
  CMakeLists.txt
```

---

## How It Works

1. **Parse** — `SceneParser` reads the XML file: camera, lights, materials, vertex/normal/UV arrays, texture image, and triangle mesh faces. Calls `buildBVH()` when done.
2. **BVH build** — Triangles are spatially sorted into a binary tree of AABBs using SAH. Leaf nodes hold up to 4 triangles.
3. **Render** — `Renderer` splits image rows across CPU threads. Each pixel generates a ray from the camera through the image plane.
4. **Trace** — For each ray:
   - Test against BVH → find closest triangle hit
   - For each light: cast shadow ray; if not blocked, accumulate diffuse + specular
   - If material is reflective: spawn reflected ray recursively (bounded by `maxraytracedepth`)
   - Blend shaded color with texture using `texturefactor`
5. **Save** — Clamp pixels to [0, 255] and write PNG or PPM.

---

## Performance

Measured on an 800×800 image:

| Scene | Triangles | Wall time |
|---|---|---|
| `scene_full_test.xml` | 8 | ~0.06 s |
| `scene_uv_debug.xml` | 2 | ~0.03 s |

For large meshes (1000+ triangles), the BVH ensures render time stays fast. Without BVH the cost scales linearly with triangle count; with BVH it scales logarithmically.
