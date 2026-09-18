"""
Generates .xmesh primitive assets - the authoring side of the on-disk mesh
format documented in Code/Modules/Xen/MeshAsset.hpp. Kept in sync with that
header by hand, since this script has no access to the C++ struct definition;
if MeshAssetHeader/MeshAssetVertex ever change, update MESH_HEADER_FORMAT and
MESH_VERTEX_FORMAT below to match.

Layout: header, then VertexCount vertices, then IndexCount indices - no
padding between sections. See MeshAsset.hpp for the exact field meanings.
"""

import struct
import sys
from pathlib import Path

MESH_ASSET_MAGIC = 0x4853584D  # 'MXSH', little-endian
MESH_ASSET_VERSION = 1

# u32 Magic, u32 Version, u32 VertexCount, u32 IndexCount, u32 IndexStride
MESH_HEADER_FORMAT = "<5I"
# f32 Position[3], f32 Normal[3], f32 Tangent[3], f32 UV[2]
MESH_VERTEX_FORMAT = "<11f"


def cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def scale(a, s):
    return tuple(x * s for x in a)


def build_cube(half_extent: float = 0.5):
    """A 24-vertex cube (one set of 4 vertices per face, not shared across
    faces - each face needs its own normal/tangent/UV, so vertices can't be
    welded the way a purely positional mesh could).

    Each face is built from a normal and two axes U/V chosen so that
    cross(U, V) == normal (the right-hand rule). Ordering the 4 corners as
    center-U-V, center+U-V, center+U+V, center-U+V then gives triangles that
    wind counter-clockwise as seen from outside the cube (looking along
    -normal) - a fact about the 2D projection of the triangle, independent of
    whether the overall coordinate system is left- or right-handed.
    """
    faces = [
        # normal,        U (tangent),   V
        ((1, 0, 0), (0, 1, 0), (0, 0, 1)),  # +X
        ((-1, 0, 0), (0, 0, 1), (0, 1, 0)),  # -X
        ((0, 1, 0), (0, 0, 1), (1, 0, 0)),  # +Y
        ((0, -1, 0), (1, 0, 0), (0, 0, 1)),  # -Y
        ((0, 0, 1), (1, 0, 0), (0, 1, 0)),  # +Z
        ((0, 0, -1), (0, 1, 0), (1, 0, 0)),  # -Z
    ]

    vertices = []
    indices = []

    for normal, u, v in faces:
        center = scale(normal, half_extent)
        half_u = scale(u, half_extent)
        half_v = scale(v, half_extent)

        corners = [
            (add(add(center, scale(half_u, -1)), scale(half_v, -1)), (0.0, 0.0)),
            (add(add(center, half_u), scale(half_v, -1)), (1.0, 0.0)),
            (add(add(center, half_u), half_v), (1.0, 1.0)),
            (add(add(center, scale(half_u, -1)), half_v), (0.0, 1.0)),
        ]

        base = len(vertices)
        for position, uv in corners:
            vertices.append((position, normal, u, uv))

        indices += [base + 0, base + 1, base + 2, base + 0, base + 2, base + 3]

    assert cross((1, 0, 0), (0, 1, 0)) == (0, 0, 1), "cross() sanity check failed"

    return vertices, indices


def write_mesh(path: Path, vertices, indices):
    index_stride = 2 if len(vertices) <= 0xFFFF else 4
    index_format = "<H" if index_stride == 2 else "<I"

    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "wb") as f:
        f.write(
            struct.pack(
                MESH_HEADER_FORMAT,
                MESH_ASSET_MAGIC,
                MESH_ASSET_VERSION,
                len(vertices),
                len(indices),
                index_stride,
            )
        )

        for position, normal, tangent, uv in vertices:
            f.write(struct.pack(MESH_VERTEX_FORMAT, *position, *normal, *tangent, *uv))

        for index in indices:
            f.write(struct.pack(index_format, index))

    print(f"✔ Wrote '{path}' ({len(vertices)} vertices, {len(indices)} indices, {index_stride}-byte indices)")


def generate_primitive_meshes(output_dir: Path):
    cube_vertices, cube_indices = build_cube()
    write_mesh(output_dir / "cube.xmesh", cube_vertices, cube_indices)


if __name__ == "__main__":
    print("---[ generate_primitive_meshes.py ]---")
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("Code") / "XenPBRDemo" / "Content" / "meshes"
    generate_primitive_meshes(out_dir)
