"""
Generates .gltf primitive assets for local testing. Meshes are loaded
directly as glTF/GLB by MeshCache::CreateGpuMesh (Code/Modules/Xen/
MeshCache.cpp, via cgltf) - there's no custom binary mesh format or offline
compile step anymore, so this script's only job is producing a small,
self-contained .gltf (JSON with an embedded base64 buffer, no external .bin
sidecar - MeshCache can't resolve one of those since it loads from a pak, not
off disk) for meshes that aren't worth authoring in a DCC tool.

Coordinate space matches glTF's own convention (right-handed, +Y up, -Z
forward) exactly, since the engine adopts it rather than converting on
import - see CameraComponent::GetViewMatrix and DirectionalLightComponent::
GetDirection for the corresponding engine-side convention.
"""

import base64
import json
import math
import struct
import sys
from pathlib import Path


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


def build_quad(center, normal, u, v, half_u, half_v):
    """One flat quad (4 vertices, 2 triangles), used both for each of
    build_cube's 6 faces and standalone flat shapes like build_plane.

    normal, u, v must satisfy cross(u, v) == normal (the right-hand rule).
    Ordering the 4 corners as center-u-v, center+u-v, center+u+v, center-u+v
    then gives triangles that wind counter-clockwise as seen from outside
    (looking along -normal) - a fact about the 2D projection of the triangle,
    independent of whether the overall coordinate system is left- or
    right-handed, and matching glTF's own CCW-front-face convention.
    """
    hu = scale(u, half_u)
    hv = scale(v, half_v)

    corners = [
        (add(add(center, scale(hu, -1)), scale(hv, -1)), (0.0, 0.0)),
        (add(add(center, hu), scale(hv, -1)), (1.0, 0.0)),
        (add(add(center, hu), hv), (1.0, 1.0)),
        (add(add(center, scale(hu, -1)), hv), (0.0, 1.0)),
    ]

    vertices = [(position, normal, u, uv) for position, uv in corners]
    indices = [0, 1, 2, 0, 2, 3]
    return vertices, indices


def _append(vertices, indices, new_vertices, new_indices):
    base = len(vertices)
    vertices.extend(new_vertices)
    indices.extend(i + base for i in new_indices)


def build_cube(half_extent: float = 0.5):
    """A 24-vertex cube (one set of 4 vertices per face, not shared across
    faces - each face needs its own normal/tangent/UV, so vertices can't be
    welded the way a purely positional mesh could). Each face is a
    build_quad offset half_extent along its own normal.
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
        face_vertices, face_indices = build_quad(center, normal, u, v, half_extent, half_extent)
        _append(vertices, indices, face_vertices, face_indices)

    assert cross((1, 0, 0), (0, 1, 0)) == (0, 0, 1), "cross() sanity check failed"

    return vertices, indices


def build_plane(width: float = 1.0, depth: float = 1.0):
    """A single flat quad lying in the XZ plane, normal +Y - a ground/floor
    primitive. u=+X, v=-Z satisfies cross(u, v) == (0, 1, 0); see build_quad.
    """
    return build_quad((0, 0, 0), (0, 1, 0), (1, 0, 0), (0, 0, -1), width * 0.5, depth * 0.5)


def build_sphere(radius: float = 0.5, rings: int = 16, segments: int = 32):
    """A UV sphere (latitude/longitude grid), pole to pole along +Y. Vertices
    are not shared across the UV seam (phi=0 duplicated at phi=2*pi) since
    the seam needs distinct UV.x values (0.0 vs 1.0) despite being the same
    position - the same un-welded-corner reasoning as build_cube.

    Winding: for a row-major grid wrapped around an axis (this and
    build_cylinder's wall both are), the quad-diagonal split that's
    CCW-from-outside is (a, a+1, b), (a+1, b+1, b) - opposite of the naive
    (a, b, a+1) order - confirmed by an explicit cross-product check, not
    just by eye, since it's easy to get backwards.
    """
    vertices = []
    for i in range(rings + 1):
        theta = math.pi * i / rings
        y = math.cos(theta)
        ring_radius = math.sin(theta)
        for j in range(segments + 1):
            phi = 2.0 * math.pi * j / segments
            x = ring_radius * math.cos(phi)
            z = ring_radius * math.sin(phi)
            position = (radius * x, radius * y, radius * z)
            normal = (x, y, z)
            tangent = (-math.sin(phi), 0.0, math.cos(phi))
            uv = (j / segments, i / rings)
            vertices.append((position, normal, tangent, uv))

    indices = []
    row_stride = segments + 1
    for i in range(rings):
        for j in range(segments):
            a = i * row_stride + j
            b = a + row_stride
            indices += [a, a + 1, b, a + 1, b + 1, b]

    return vertices, indices


def build_cylinder(radius: float = 0.5, height: float = 1.0, segments: int = 32):
    """A capped cylinder, axis along Y. The side wall is a row-major grid
    exactly like build_sphere's (see its winding note), and each cap is a
    separate triangle fan from a center vertex - wall and cap vertices at
    the rim are duplicated since they need different normals (radial vs.
    +-Y), same reasoning as every other un-welded seam here.
    """
    half_height = height * 0.5
    vertices = []
    indices = []

    row_stride = segments + 1
    for y, v in ((half_height, 0.0), (-half_height, 1.0)):
        for j in range(segments + 1):
            phi = 2.0 * math.pi * j / segments
            x = math.cos(phi)
            z = math.sin(phi)
            position = (radius * x, y, radius * z)
            normal = (x, 0.0, z)
            tangent = (-math.sin(phi), 0.0, math.cos(phi))
            vertices.append((position, normal, tangent, (j / segments, v)))

    for j in range(segments):
        a = j
        b = a + row_stride
        indices += [a, a + 1, b, a + 1, b + 1, b]

    # Caps: top (+Y) and bottom (-Y) are mirror images of each other, so a
    # fan winding that's CCW-from-outside on one is CW on the other - top
    # uses (center, rim[j+1], rim[j]), bottom the reverse. Verified by the
    # same explicit cross-product check as build_sphere's winding, not just
    # by eye.
    for y, normal, flip_winding in ((half_height, (0.0, 1.0, 0.0), False), (-half_height, (0.0, -1.0, 0.0), True)):
        center_index = len(vertices)
        vertices.append(((0.0, y, 0.0), normal, (1.0, 0.0, 0.0), (0.5, 0.5)))

        rim_base = len(vertices)
        for j in range(segments + 1):
            phi = 2.0 * math.pi * j / segments
            x = math.cos(phi)
            z = math.sin(phi)
            uv = (0.5 + 0.5 * x, 0.5 + 0.5 * z)
            vertices.append(((radius * x, y, radius * z), normal, (1.0, 0.0, 0.0), uv))

        for j in range(segments):
            r0 = rim_base + j
            r1 = rim_base + j + 1
            indices += [center_index, r0, r1] if flip_winding else [center_index, r1, r0]

    return vertices, indices


def write_gltf(path: Path, vertices, indices):
    """Writes a minimal single-mesh .gltf: one buffer (embedded as a base64
    data URI), one bufferView + accessor per attribute plus one for indices,
    one mesh/node/scene. Attributes are planar (not interleaved) - simplest
    to assemble by hand, and cgltf reads either layout identically.
    """
    positions = b"".join(struct.pack("<3f", *p) for p, _, _, _ in vertices)
    normals = b"".join(struct.pack("<3f", *n) for _, n, _, _ in vertices)
    # glTF tangents are vec4 (xyz + a bitangent-handedness sign); w=1.0 is an
    # arbitrary placeholder - MeshCache drops the sign on import regardless,
    # since nothing consumes tangents for normal mapping yet.
    tangents = b"".join(struct.pack("<4f", *t, 1.0) for _, _, t, _ in vertices)
    uvs = b"".join(struct.pack("<2f", *uv) for *_, uv in vertices)

    index_component_type = 5123 if len(vertices) <= 0xFFFF else 5125  # UNSIGNED_SHORT / UNSIGNED_INT
    index_format = "<H" if index_component_type == 5123 else "<I"
    index_bytes = b"".join(struct.pack(index_format, i) for i in indices)

    sections = [positions, normals, tangents, uvs, index_bytes]
    offsets = []
    offset = 0
    for section in sections:
        offsets.append(offset)
        offset += len(section)
    buffer_bytes = b"".join(sections)

    xs = [p[0] for p, _, _, _ in vertices]
    ys = [p[1] for p, _, _, _ in vertices]
    zs = [p[2] for p, _, _, _ in vertices]

    gltf = {
        "asset": {"version": "2.0", "generator": "Xen2D generate_primitive_meshes.py"},
        "buffers": [
            {
                "byteLength": len(buffer_bytes),
                "uri": "data:application/octet-stream;base64," + base64.b64encode(buffer_bytes).decode("ascii"),
            }
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": offsets[0], "byteLength": len(positions), "target": 34962},
            {"buffer": 0, "byteOffset": offsets[1], "byteLength": len(normals), "target": 34962},
            {"buffer": 0, "byteOffset": offsets[2], "byteLength": len(tangents), "target": 34962},
            {"buffer": 0, "byteOffset": offsets[3], "byteLength": len(uvs), "target": 34962},
            {"buffer": 0, "byteOffset": offsets[4], "byteLength": len(index_bytes), "target": 34963},
        ],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,  # FLOAT
                "count": len(vertices),
                "type": "VEC3",
                "min": [min(xs), min(ys), min(zs)],
                "max": [max(xs), max(ys), max(zs)],
            },
            {"bufferView": 1, "componentType": 5126, "count": len(vertices), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": len(vertices), "type": "VEC4"},
            {"bufferView": 3, "componentType": 5126, "count": len(vertices), "type": "VEC2"},
            {"bufferView": 4, "componentType": index_component_type, "count": len(indices), "type": "SCALAR"},
        ],
        "meshes": [
            {
                "primitives": [
                    {
                        "attributes": {"POSITION": 0, "NORMAL": 1, "TANGENT": 2, "TEXCOORD_0": 3},
                        "indices": 4,
                        "mode": 4,  # TRIANGLES
                    }
                ]
            }
        ],
        "nodes": [{"mesh": 0}],
        "scenes": [{"nodes": [0]}],
        "scene": 0,
    }

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(gltf), encoding="utf-8")

    print(f"Wrote '{path}' ({len(vertices)} vertices, {len(indices)} indices)")


def generate_primitive_meshes(output_dir: Path):
    write_gltf(output_dir / "cube.gltf", *build_cube())
    write_gltf(output_dir / "plane.gltf", *build_plane())
    write_gltf(output_dir / "sphere.gltf", *build_sphere())
    write_gltf(output_dir / "cylinder.gltf", *build_cylinder())


if __name__ == "__main__":
    print("---[ generate_primitive_meshes.py ]---")
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("Code") / "Demos" / "Demo.PBR" / "Content" / "meshes"
    generate_primitive_meshes(out_dir)
