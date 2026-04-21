#!/usr/bin/env python3
"""Utility script that generates the test assets committed under scenes/.

Outputs:
  - scenes/checker.ppm: 256x256 checkerboard texture for textured_cube.xml
  - scenes/grid_large.xml: a tessellated terrain-like scene with a few
    thousand triangles so we can measure renderer performance.

Re-run whenever you want to regenerate the assets; the ray tracer itself
does not depend on Python.
"""

import os
import struct
import math

HERE = os.path.dirname(os.path.abspath(__file__))
SCENES = os.path.join(HERE, os.pardir, "scenes")

# ---------------------------------------------------------------------------
# Texture: 256x256 checkerboard (orange/teal) as PPM P6.
# ---------------------------------------------------------------------------
def write_checker():
    path = os.path.join(SCENES, "checker.ppm")
    W, H = 256, 256
    tile = 32
    with open(path, "wb") as f:
        f.write(f"P6\n{W} {H}\n255\n".encode())
        row = bytearray(W * 3)
        for y in range(H):
            for x in range(W):
                dark = ((x // tile) + (y // tile)) % 2 == 0
                if dark:
                    r, g, b = 220, 150, 60
                else:
                    r, g, b = 40, 140, 160
                row[x*3] = r
                row[x*3 + 1] = g
                row[x*3 + 2] = b
            f.write(row)
    print(f"Wrote {path} ({W}x{H})")


# ---------------------------------------------------------------------------
# Grid scene: a tessellated ground plane plus hundreds of small "bump" quads
# that bounce sine waves so the scene is non-trivial.
# Triangle count = 2 * NxN for the ground + two cubes = ~2000+.
# ---------------------------------------------------------------------------
def write_grid_scene(N=32):
    path = os.path.join(SCENES, "grid_large.xml")
    # Ground plane spans [-W_EXT, W_EXT] in x/z with N subdivisions.
    W_EXT = 8.0
    # Height field: small sine bumps for visual interest.
    def height(x, z):
        return 0.15 * math.sin(1.2 * x) * math.cos(1.1 * z) + \
               0.08 * math.sin(2.3 * (x + z))

    vertices = []
    normals = []

    # Grid vertices (N+1)x(N+1).
    for j in range(N + 1):
        for i in range(N + 1):
            t_x = i / N
            t_z = j / N
            x = -W_EXT + 2 * W_EXT * t_x
            z = -W_EXT + 2 * W_EXT * t_z
            y = height(x, z) - 0.8  # sit slightly below origin
            vertices.append((x, y, z))

    # Compute per-vertex normals from central differences.
    eps = 1.0
    def vertexNormal(i, j):
        x = -W_EXT + 2 * W_EXT * (i / N)
        z = -W_EXT + 2 * W_EXT * (j / N)
        dydx = (height(x + eps, z) - height(x - eps, z)) / (2 * eps)
        dydz = (height(x, z + eps) - height(x, z - eps)) / (2 * eps)
        nx, ny, nz = -dydx, 1.0, -dydz
        L = math.sqrt(nx*nx + ny*ny + nz*nz)
        return (nx/L, ny/L, nz/L)

    for j in range(N + 1):
        for i in range(N + 1):
            normals.append(vertexNormal(i, j))

    # Triangles from quads, CCW viewed from above.
    # Grid vertex index = j*(N+1) + i + 1 (1-based).
    faces = []
    for j in range(N):
        for i in range(N):
            a = j * (N + 1) + i + 1
            b = j * (N + 1) + (i + 1) + 1
            c = (j + 1) * (N + 1) + (i + 1) + 1
            d = (j + 1) * (N + 1) + i + 1
            faces.append((a, b, c))
            faces.append((a, c, d))
    ground_triangles = len(faces)

    # Two cubes added at fixed positions.
    cube_verts_base = [
        (-0.8, -0.8, -0.8), ( 0.8, -0.8, -0.8),
        ( 0.8, -0.8,  0.8), (-0.8, -0.8,  0.8),
        (-0.8,  0.8, -0.8), ( 0.8,  0.8, -0.8),
        ( 0.8,  0.8,  0.8), (-0.8,  0.8,  0.8),
    ]
    # Cube normals: 6 face normals appended after the grid normals.
    cube_normals = [
        ( 0, -1,  0), ( 0,  1,  0),
        ( 0,  0,  1), ( 0,  0, -1),
        ( 1,  0,  0), (-1,  0,  0),
    ]

    cubes = [
        ("cube1", ( 2.5, 0.0, -1.0), "red"),
        ("cube2", (-2.0, 0.0,  1.5), "blue"),
    ]

    cube_meshes = []
    for name, (cx, cy, cz), mat in cubes:
        base = len(vertices) + 1  # first vertex index (1-based) for this cube
        for (vx, vy, vz) in cube_verts_base:
            vertices.append((vx + cx, vy + cy - 0.2, vz + cz))
        # 12 faces with face-normal indices relative to cube_normal_base.
        # cube_normal_base: set after we decide where to append normals.
        cube_meshes.append((name, mat, base))

    # Append cube face normals after the grid normals.
    cube_normal_base = len(normals) + 1
    for n in cube_normals:
        normals.append(n)

    # Emit cube faces now that we know the normal base index.
    cube_face_defs = [
        (1, 2, 3, 1), (1, 3, 4, 1),
        (5, 8, 7, 2), (5, 7, 6, 2),
        (4, 3, 7, 3), (4, 7, 8, 3),
        (2, 1, 5, 4), (2, 5, 6, 4),
        (2, 6, 7, 5), (2, 7, 3, 5),
        (1, 4, 8, 6), (1, 8, 5, 6),
    ]
    cube_mesh_blocks = []
    for (name, mat, base) in cube_meshes:
        block_faces = []
        for (a, b, c, n) in cube_face_defs:
            ni = cube_normal_base + (n - 1)
            block_faces.append(f"{base + a - 1}//{ni} {base + b - 1}//{ni} {base + c - 1}//{ni}")
        cube_mesh_blocks.append((name, mat, block_faces))
    cube_triangles_total = len(cube_face_defs) * len(cubes)

    # ---- Write XML ----
    vbuf = "\n".join(f"        {v[0]:.5f} {v[1]:.5f} {v[2]:.5f}" for v in vertices)
    nbuf = "\n".join(f"        {n[0]:.5f} {n[1]:.5f} {n[2]:.5f}" for n in normals)

    # Ground mesh uses interpolated vertex normals: v//n where normal index = vertex index.
    ground_face_strs = []
    for (a, b, c) in faces:
        ground_face_strs.append(f"{a}//{a} {b}//{b} {c}//{c}")
    ground_faces_text = "\n                ".join(ground_face_strs)

    cube_mesh_xml = []
    for (name, mat, block_faces) in cube_mesh_blocks:
        body = "\n                ".join(block_faces)
        cube_mesh_xml.append(
            f"""        <mesh id=\"{name}\">
            <materialid>{mat}</materialid>
            <faces>
                {body}
            </faces>
        </mesh>"""
        )
    cube_mesh_xml_text = "\n".join(cube_mesh_xml)

    total_tris = ground_triangles + cube_triangles_total

    xml = f"""<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!-- Generated by tools/gen_assets.py. Grid: {N}x{N} quads = {ground_triangles} triangles.
     Total: {total_tris} triangles across ground + {len(cubes)} cubes. -->
<scene>
    <maxraytracedepth>4</maxraytracedepth>
    <background>12 18 35</background>

    <camera>
        <position>0 5 9</position>
        <gaze>0 -0.45 -0.89</gaze>
        <up>0 1 0</up>
        <nearplane>-0.5 0.5 -0.5 0.5</nearplane>
        <neardistance>1</neardistance>
        <imageresolution>800 600</imageresolution>
    </camera>

    <lights>
        <ambientlight>35 35 40</ambientlight>
        <pointlight id=\"sun\">
            <position>6 10 6</position>
            <intensity>28000 26000 22000</intensity>
        </pointlight>
        <pointlight id=\"fill\">
            <position>-6 4 -2</position>
            <intensity>6000 6000 10000</intensity>
        </pointlight>
    </lights>

    <materials>
        <material id=\"ground\">
            <ambient>0.35 0.4 0.3</ambient>
            <diffuse>0.55 0.6 0.45</diffuse>
            <specular>0.08 0.08 0.08</specular>
            <mirrorreflectance>0 0 0</mirrorreflectance>
            <phongexponent>12</phongexponent>
            <texturefactor>0</texturefactor>
        </material>
        <material id=\"red\">
            <ambient>0.35 0.1 0.1</ambient>
            <diffuse>0.9 0.2 0.2</diffuse>
            <specular>0.4 0.4 0.4</specular>
            <mirrorreflectance>0 0 0</mirrorreflectance>
            <phongexponent>30</phongexponent>
            <texturefactor>0</texturefactor>
        </material>
        <material id=\"blue\">
            <ambient>0.1 0.1 0.35</ambient>
            <diffuse>0.2 0.3 0.9</diffuse>
            <specular>0.5 0.5 0.6</specular>
            <mirrorreflectance>0.1 0.1 0.15</mirrorreflectance>
            <phongexponent>60</phongexponent>
            <texturefactor>0</texturefactor>
        </material>
    </materials>

    <vertexdata>
{vbuf}
    </vertexdata>

    <normaldata>
{nbuf}
    </normaldata>

    <objects>
        <mesh id=\"ground\">
            <materialid>ground</materialid>
            <faces>
                {ground_faces_text}
            </faces>
        </mesh>
{cube_mesh_xml_text}
    </objects>
</scene>
"""

    with open(path, "w") as f:
        f.write(xml)
    print(f"Wrote {path} with {total_tris} triangles ({ground_triangles} in the ground + {cube_triangles_total} cube tris)")


if __name__ == "__main__":
    write_checker()
    write_grid_scene(N=32)
