# Triangle Ray Tracer (PA1 Ready)

This project is a C++17 ray tracer that reads an XML scene file and writes a rendered image in PPM or PNG format.

## Implemented Features

- Triangle-only geometry (`mesh` faces as triangles)
- Ray-triangle intersection (Moller-Trumbore)
- Shadow rays with numeric offset (`shadowBias`)
- Ambient + diffuse + specular (Phong)
- Mirror reflection with recursive ray tracing depth
- Texture sampling with material `texturefactor` blending
  - `texturefactor = 1.0`: direct texture color, no shading contribution
  - `texturefactor = 0.0`: texture does not affect final color
- Point lights
- Triangular directional lights
  - Direction follows `(vertex1-vertex2) x (vertex1-vertex3)`
- Multi-threaded rendering (uses hardware concurrency)

## Build

### Make

```bash
cd /Users/mertugur/cgodev
make -j4
```

### CMake

```bash
cd /Users/mertugur/cgodev
cmake -S . -B build
cmake --build build -j4
```

## Run

```bash
cd /Users/mertugur/cgodev
./raytracer scene1.xml output.ppm
./raytracer scene1.xml output.png
```

Open output on macOS:

```bash
open output.ppm
```

## Test Scenes

- `scene_test.xml`: minimal sanity scene
- `scene_full_test.xml`: feature coverage scene (materials, texturefactor, mirror, multiple lights)
- `scene_1000_triangles.xml`: performance/scale scene with exactly 1000 triangles

## Smoke Tests

```bash
cd /Users/mertugur/cgodev
./scripts/run_smoke_tests.sh
```

## Performance Example

Measured on this machine for 800x800 with 1000 triangles:

```text
./raytracer scene_1000_triangles.xml output_1000_ready.ppm
~0.20s wall time
```

## Project Structure

- `include/`: headers
- `src/`: implementation
- `scripts/run_smoke_tests.sh`: quick verification
- `scene*.xml`: sample test scenes
- `tinyxml2.*`: XML parsing library source
