# Raytracer
A C++17 ray tracer that renders scenes. Implements ray-triangle intersection only (no sphere / cylinder / plane code), with BVH acceleration structure and multithreaded rendering.

## Build

```sh
make              # produces ./raytracer (optimized build)
make debug        # -O0 -g
make clean
```

Requires a C++17 compiler with `<thread>` (Apple clang 17 / g++ 9+).

## Run

```sh
./raytracer <scene.xml> [output.png|.ppm|.bmp|.tga|.jpg] [threads]
```

- When the output path is omitted, `<scene>.png` is written next to the
  scene. The encoder is picked from the file extension: `.png`, `.bmp`,
  `.tga`, `.jpg`/`.jpeg`, anything else falls back to PPM P6.
- `threads` defaults to `std::thread::hardware_concurrency()`; pass `1`
  to render single-threaded.
- Texture images can be any of PNG, JPG, BMP, TGA, GIF, PSD, PIC, PNM
  (loaded via [stb_image](https://github.com/nothings/stb)). The file is
  resolved relative to the scene XML's directory.

The helper script `tools/gen_assets.py` regenerates `checker.ppm` and
the large 2k-triangle test scene.

## Project layout

```
src/
  Vec3.h, Ray.h, AABB.h    header-only math primitives
  Material.h, Light.h       struct definitions for scene data
  Camera.h                  camera basis setup + primary-ray generation
  Mesh.h                    Triangle, Mesh, Hit structs
  Image.h / Image.cpp       stb_image loader (PNG/JPG/BMP/TGA/PPM/...) + writer
  stb_image.h, stb_image_write.h, stb_impl.cpp  vendored public-domain codecs
  XmlParser.h/.cpp          small tolerant XML-subset parser
  Scene.h/.cpp              loads the XML scene into typed structs
  BVH.h/.cpp                binary BVH, median-split on longest centroid axis
  RayTracer.h/.cpp          shading, recursion, multithreaded render loop
  main.cpp                  CLI entry point with timing output
scenes/
  cube_simple.xml           single cube + point light (12 triangles)
  two_cubes.xml             two cubes + floor + triangular light (26 triangles)
  mirror_scene.xml          reflective cube + gold cube + floor (26 triangles)
  textured_cube.xml         cube with checker.ppm texture, texturefactor=0.7
  textured_cube_png.xml     same scene, referencing checker.png (stb_image)
  grid_large.xml            heightfield terrain + 2 cubes (2072 triangles)
  checker.ppm, checker.png  256×256 checkerboard texture (both formats)
tools/
  gen_assets.py             regenerates checker.ppm and grid_large.xml
Makefile                    builds ./raytracer
```

## What is implemented

### XML scene description

`Scene::load` (in `src/Scene.cpp`) parses every element in the
xml:

- `<maxraytracedepth>` (recursion cap for mirror reflection)
- `<background>` — also accepts the PDF's example typo `<backgroundColor>`
- `<camera>` with `<position>`, `<gaze>`, `<up>`, `<nearplane>` /
  `nearPlane`, `<neardistance>`, `<imageresolution>`
- `<lights>` containing `<ambientlight>`, one or more `<pointlight>` and
  `<triangularlight>`
- `<materials>` with `<ambient>`, `<diffuse>`, `<specular>`,
  `<phongexponent>`, `<mirrorreflectance>`, `<texturefactor>`
- `<vertexdata>`, `<texturedata>`, `<normaldata>` (whitespace-tolerant,
  any number of triples / pairs)
- `<textureimage>` (loaded relative to the scene file via stb_image;
  accepts PNG, JPG, BMP, TGA, GIF, PSD, PIC, PNM)
- `<objects>` containing one or more `<mesh>`, each with `<materialid>`
  and `<faces>`. Face tokens use the OBJ-style `v/t/n`, `v//n`, `v/t`,
  or `v`. Indices are 1-based and are converted to 0-based internally.

The parser accepts material ids either by string id or by numeric id
matching the example scene in the PDF. Unknown or numeric indices fall
through to the integer fallback (`<materialid>1</materialid>`).

### Camera + primary rays

`Camera::setup` builds an orthonormal basis `u, v, w` where `w = -gaze`,
`u = up × w`, `v = w × u`. `primaryRay(i, j)` computes the image plane
point
`s = e − d·w + s_u·u + s_v·v` with `s_u, s_v` derived from the `nearplane`
extents and the pixel coordinates, then emits a normalized ray from the
camera position (`src/Camera.h`).

### Ray / triangle intersection

Möller–Trumbore in `BVH::intersectTriangle` (the only primitive we need).
It returns `t`, barycentric `(u, v)`, and reports the front hit only
when `t > 0`. Triangles have precomputed face normals stored at load
time for fast offset / backface handling.

### BVH

`BVH::build` is a top-down binary split:

1. Compute the triangle AABB and centroid AABB for the active range.
2. Pick the longest axis of the centroid AABB.
3. Use `std::nth_element` to partition triangles around the median
   centroid on that axis (O(N) per level, O(N log N) total).
4. Stop when a leaf would have ≤ 4 triangles (`kLeafSize`).

Traversal (`BVH::intersect`, `BVH::occluded`) is iterative with a small
stack; at each internal node it pushes the farther child first so the
nearer one is visited next. Shadow rays use the any-hit variant and
exit as soon as any intersection with `tMin < t < tMax` is found.

On a 2072-triangle scene at 800×600 the BVH reduces per-pixel work to
`O(log N)` intersections, bringing render time from hundreds of
milliseconds (brute force) to tens of milliseconds even with recursion.

### Shading (Blinn–Phong) and recursion

`RayTracer::shade` implements the following per-hit model:

- Ambient: `L_a ⊗ k_a`
- For every point light with unobstructed shadow ray:
  - `irradiance = I / d²`
  - `color += k_d ⊗ irradiance · max(0, n·l)`
  - `color += k_s ⊗ irradiance · max(0, n·h)^p` (Blinn–Phong, `h =
    normalize(l + v)`)
- For every triangular light: same as a point light using the centroid
  as the sample point, but only if the shaded point lies on the
  emission side of the light plane (direction is `normalize((v1−v2) ×
  (v1−v3))`, per the spec).
- Mirror reflection: if `mirrorreflectance` is non-zero and
  `ray.depth < maxraytracedepth`, spawn a reflection ray `r = d − 2(n·d)n`
  and add `k_m ⊗ traceRay(reflection)` to the color. Reflection rays
  that escape the scene sample the background color, so a mirror that
  sees the sky shows the sky.
- Texture blend (after lighting): `final = tf · tex + (1 − tf) · shaded`.
  This matches the spec: at `tf = 1` the texture is used directly and
  shading does not alter the color; at `tf = 0` the texture has no
  effect.

Shading normals are smoothed via barycentric interpolation of the three
vertex normals when they are provided (the heightfield ground in
`grid_large.xml` uses this). If per-vertex normals are not referenced,
the precomputed face normal is used. Normals are flipped to face the
ray to render the back side of open meshes correctly.

### Self-intersection offset

The shadow / reflection ray origin is offset along the shading normal
by `kShadowEpsilon = 1e-3` (`RayTracer::kShadowEpsilon`) to avoid
numeric self-intersection. The BVH also biases `tMin` by `1e-4` for the
same reason. These constants live in a single spot so they can be
retuned if needed.

### Multithreading

`RayTracer::render` starts `hardware_concurrency()` (or a user
override) `std::thread`s sharing an `std::atomic<int>` that hands out
contiguous row bands. Band size defaults to `H / (threads · 8)` so
threads do not go idle when an early band finishes quickly (e.g., a
row that is mostly background). Pixels are written directly into a
shared `Image`; since each pixel is owned by a single thread, no
locking is needed.

### Image I/O

`loadImage` decodes textures via the vendored
[stb_image](https://github.com/nothings/stb) (PNG, JPG, BMP, TGA, GIF,
PSD, PIC) and falls back to a built-in PPM P3/P6 reader with arbitrary
`maxval` for PNM files and any PPM variants stb happens to reject.
`writeImage` picks the encoder from the output extension: `.png`,
`.bmp`, `.tga`, `.jpg`/`.jpeg`, or PPM P6 as a fallback. Bilinear
sampling wraps u/v into `[0, 1)`, flips v so `(0,0)` is bottom-left
(standard OpenGL convention), and interpolates across the four nearest
texels.

## Performance

Measured on an Apple-silicon Mac (14 hardware threads). All render
times exclude scene loading and BVH build (both are reported separately
by the binary).

| Scene             | Triangles | 1 thread | 2 threads | 4 threads | 14 threads |
|-------------------|-----------|---------:|----------:|----------:|-----------:|
| cube_simple       |       12  |   9.7 ms |    5.0 ms |    2.6 ms |     1.2 ms |
| textured_cube     |       12  |  14.2 ms |    7.2 ms |    3.7 ms |     1.6 ms |
| two_cubes         |       26  |  63.6 ms |   33.6 ms |   17.4 ms |     7.1 ms |
| mirror_scene      |       26  |  96.7 ms |   50.0 ms |   26.7 ms |    11.7 ms |
| grid_large        |    2,072  | 142.4 ms |   73.6 ms |   41.9 ms |    16.2 ms |
| grid_large (96×96) |  18,456  | 204.5 ms |         – |         – |    22.3 ms |

Observations:

- Thread scaling is close to linear up to the physical core count
  (≈ 9× at 14 threads on `grid_large`). The atomic row-band dispatcher
  keeps load balance even when some rows hit only background.
- Raising the ground tessellation from 32×32 (2,072 triangles) to 96×96
  (18,456 triangles) grows render time by only ≈ 1.4× because the BVH
  keeps per-ray intersection tests at `O(log N)`. Without the BVH the
  same scene would scale linearly in triangle count (≈ 9× slower).
- `mirror_scene` is measurably slower than `two_cubes` at the same
  triangle count because reflection rays fire a second traversal and
  shading call per hit, up to `maxraytracedepth` times.
- Build time for the 18k-triangle BVH is ≈ 4 ms — negligible compared
  to the render itself.

## Running the included scenes

```sh
# quick smoke tests
make run-simple           # scenes/cube_simple.xml
make run-mirror           # scenes/mirror_scene.xml
make run-many             # scenes/grid_large.xml

# explicit arguments, 4 threads, custom output
./raytracer scenes/two_cubes.xml out.png 4

# or pick a different format by changing the extension
./raytracer scenes/two_cubes.xml out.jpg
./raytracer scenes/two_cubes.xml out.ppm
```

Regenerate the texture + large scene:

```sh
python3 tools/gen_assets.py
```

## Limitations / notes

- Image I/O uses the vendored single-header
  [stb_image](https://github.com/nothings/stb) and `stb_image_write`
  libraries. Input textures may be PNG, JPG, BMP, TGA, GIF, PSD, PIC,
  or PNM (P3/P6); output is chosen by the file extension
  (`.png`/`.bmp`/`.tga`/`.jpg`/`.jpeg`) with a binary PPM P6 fallback
  for anything else.
- `triangularlight` is sampled at its centroid (one deterministic
  shadow ray per light per hit). This matches the spec's "planar
  directional light" description and gives clean images; multi-sample
  area lights would be a straightforward extension.
- The BVH uses median splits rather than SAH. Median is cheap to build
  and good enough for the scenes; SAH would mainly
  help on very non-uniform scenes.
