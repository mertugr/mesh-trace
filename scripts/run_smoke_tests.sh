#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

make -j4 >/dev/null

scenes=(
  "scene_test.xml"
  "scene_full_test.xml"
  "scene_1000_triangles.xml"
)

for scene in "${scenes[@]}"; do
  out="${scene%.xml}_smoke.ppm"
  ./raytracer "$scene" "$out" >/dev/null
  if [[ ! -s "$out" ]]; then
    echo "[FAIL] Output missing or empty for $scene"
    exit 1
  fi

  header=$(head -c 2 "$out")
  if [[ "$header" != "P6" ]]; then
    echo "[FAIL] Invalid PPM header for $out"
    exit 1
  fi

  echo "[OK] $scene -> $out"
done

echo "All smoke tests passed."
