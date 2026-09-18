//
// Created by Jake Rieger on 9/17/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

namespace Xen {
    // On-disk mesh format (.xmesh). Deliberately minimal: one fixed vertex
    // layout, no materials/skinning/LODs/submeshes - just enough to get a
    // triangle mesh from an asset onto the GPU. This header is the loader's
    // side of the contract; Scripts/generate_primitive_meshes.py is the
    // authoring side, and hand-packs the identical byte layout since it has
    // no access to this header.
    //
    // Layout: MeshAssetHeader, then VertexCount * MeshAssetVertex, then
    // IndexCount * (2 or 4 byte indices per Header.IndexStride). No padding
    // between sections.
    inline constexpr u32 MESH_ASSET_MAGIC   = 0x4853584D;  // 'MXSH', little-endian
    inline constexpr u32 MESH_ASSET_VERSION = 1;

#pragma pack(push, 1)
    struct MeshAssetHeader {
        u32 Magic;
        u32 Version;
        u32 VertexCount;
        u32 IndexCount;
        u32 IndexStride;  // 2 or 4 bytes per index
    };

    struct MeshAssetVertex {
        f32 Position[3];
        f32 Normal[3];
        f32 Tangent[3];  // handedness/bitangent not stored yet - no normal mapping consumes this until it's needed
        f32 UV[2];
    };
#pragma pack(pop)

    static_assert(sizeof(MeshAssetHeader) == 20, "MeshAssetHeader must match the on-disk layout exactly");
    static_assert(sizeof(MeshAssetVertex) == 44, "MeshAssetVertex must match the on-disk layout exactly");
}  // namespace Xen
