//
// Created by Jake Rieger on 9/17/2026.
//

#include <Common/Exception.hpp>
#include <Common/Log.hpp>

#include "MeshCache.hpp"

#include <XenPAK/AssetRegistry.hpp>
#include <XenPAK/AssetBuffer.hpp>

#include <cgltf.h>

#include <format>
#include <ranges>
#include <vector>

namespace Xen {
    namespace {
        // cgltf exposes a primitive's attributes as an unordered list rather
        // than named fields; this hunts out a specific one by semantic (and,
        // for TEXCOORD, by set index - only set 0 is read, since nothing in
        // the renderer samples a second UV channel).
        const cgltf_accessor*
        FindAttribute(const cgltf_primitive& Primitive, const cgltf_attribute_type Type, const cgltf_int SetIndex = 0) {
            for (cgltf_size i = 0; i < Primitive.attributes_count; ++i) {
                const cgltf_attribute& Attr = Primitive.attributes[i];
                if (Attr.type == Type && Attr.index == SetIndex) return Attr.data;
            }
            return nullptr;
        }

        // RAII wrapper so an exception thrown partway through CreateGpuMesh
        // (a THROW_ENGINE_EXCEPTION from any of the validation checks below)
        // can't leak the cgltf_data allocation.
        struct GltfHandle {
            cgltf_data* Data {nullptr};
            ~GltfHandle() {
                if (Data) cgltf_free(Data);
            }
        };
    }  // namespace

    MeshCache::~MeshCache() {
        Clear();
    }

    MeshCache::GpuMesh MeshCache::CreateGpuMesh(const AssetID ID) const {
        if (!_Assets->Contains(ID)) {
            THROW_ENGINE_EXCEPTION(EngineException, std::format("mesh asset {} not found", ID.Value));
        }

        const PAK::AssetBuffer Encoded = _Assets->Load(ID);

        cgltf_options Options {};
        GltfHandle Gltf;
        if (cgltf_parse(&Options, Encoded.Data(), Encoded.Size(), &Gltf.Data) != cgltf_result_success) {
            THROW_ENGINE_EXCEPTION(EngineException, std::format("mesh asset {} is not a valid glTF/GLB file", ID.Value));
        }

        // No base path is passed: this asset came out of a pak, not off
        // disk, so an external sibling ".bin" file (a plain relative URI)
        // has nowhere to resolve to. GLB's embedded binary chunk and a
        // .gltf's base64 data-URI buffers both still resolve fine without
        // one - see cgltf_load_buffers - so only a multi-file .gltf+.bin
        // export is unsupported here.
        if (cgltf_load_buffers(&Options, Gltf.Data, nullptr) != cgltf_result_success) {
            THROW_ENGINE_EXCEPTION(
              EngineException,
              std::format("mesh asset {} references an external buffer file, which isn't supported from a "
                           "pak - re-export with embedded/binary buffers (GLB, or .gltf with a data URI)",
                           ID.Value));
        }

        if (Gltf.Data->meshes_count == 0 || Gltf.Data->meshes[0].primitives_count == 0) {
            THROW_ENGINE_EXCEPTION(EngineException, std::format("mesh asset {} contains no mesh data", ID.Value));
        }
        if (Gltf.Data->meshes_count > 1 || Gltf.Data->meshes[0].primitives_count > 1) {
            LOG_WARN("mesh asset %llu has more than one mesh/primitive - only the first is loaded (no submesh support yet)",
                     ID.Value);
        }

        const cgltf_primitive& Primitive = Gltf.Data->meshes[0].primitives[0];
        if (Primitive.type != cgltf_primitive_type_triangles) {
            THROW_ENGINE_EXCEPTION(EngineException, std::format("mesh asset {} uses a non-triangle primitive topology", ID.Value));
        }

        const cgltf_accessor* PositionAccessor = FindAttribute(Primitive, cgltf_attribute_type_position);
        const cgltf_accessor* NormalAccessor   = FindAttribute(Primitive, cgltf_attribute_type_normal);
        const cgltf_accessor* TangentAccessor  = FindAttribute(Primitive, cgltf_attribute_type_tangent);
        const cgltf_accessor* UVAccessor       = FindAttribute(Primitive, cgltf_attribute_type_texcoord);

        if (!PositionAccessor || !NormalAccessor) {
            THROW_ENGINE_EXCEPTION(EngineException,
                                    std::format("mesh asset {} is missing POSITION or NORMAL attributes", ID.Value));
        }

        const auto VertexCount = CAST<u32>(PositionAccessor->count);

        std::vector<MeshVertex> Vertices(VertexCount);
        for (u32 i = 0; i < VertexCount; ++i) {
            MeshVertex& V = Vertices[i];

            cgltf_accessor_read_float(PositionAccessor, i, V.Position, 3);
            cgltf_accessor_read_float(NormalAccessor, i, V.Normal, 3);

            if (TangentAccessor) {
                f32 Tangent4[4] {};
                cgltf_accessor_read_float(TangentAccessor, i, Tangent4, 4);
                V.Tangent[0] = Tangent4[0];
                V.Tangent[1] = Tangent4[1];
                V.Tangent[2] = Tangent4[2];
                // Tangent4[3] is glTF's bitangent-handedness sign - dropped,
                // matching MeshVertex::Tangent (see MeshCache.hpp).
            } else {
                V.Tangent[0] = V.Tangent[1] = V.Tangent[2] = 0.0f;
            }

            if (UVAccessor) {
                cgltf_accessor_read_float(UVAccessor, i, V.UV, 2);
            } else {
                V.UV[0] = V.UV[1] = 0.0f;
            }
        }

        // glTF allows a primitive with no `indices` at all: every 3
        // consecutive vertices form a triangle implicitly. Synthesize that
        // sequence so the renderer always has an index buffer to bind.
        const u32 IndexCount = Primitive.indices ? CAST<u32>(Primitive.indices->count) : VertexCount;
        std::vector<u32> Indices(IndexCount);
        if (Primitive.indices) {
            for (u32 i = 0; i < IndexCount; ++i) {
                Indices[i] = CAST<u32>(cgltf_accessor_read_index(Primitive.indices, i));
            }
        } else {
            for (u32 i = 0; i < IndexCount; ++i) Indices[i] = i;
        }

        const bool UseU16 = VertexCount <= 0xFFFF;
        std::vector<u16> Indices16;
        if (UseU16) {
            Indices16.resize(IndexCount);
            for (u32 i = 0; i < IndexCount; ++i) Indices16[i] = CAST<u16>(Indices[i]);
        }

        GpuMesh Mesh;
        Mesh.Info.VertexCount = VertexCount;
        Mesh.Info.IndexCount  = IndexCount;
        Mesh.Info.IndexType   = UseU16 ? RHI::IndexType::U16 : RHI::IndexType::U32;

        RHI::BufferDesc VBDesc;
        VBDesc.Size        = Vertices.size() * sizeof(MeshVertex);
        VBDesc.Usage       = RHI::BufferUsage::Vertex;
        VBDesc.Memory      = RHI::MemoryUsage::GpuOnly;
        VBDesc.InitialData = Vertices.data();
        VBDesc.DebugName   = "Mesh VB";
        Mesh.VertexBuffer  = _Device->CreateBuffer(VBDesc);

        RHI::BufferDesc IBDesc;
        IBDesc.Size        = UseU16 ? Indices16.size() * sizeof(u16) : Indices.size() * sizeof(u32);
        IBDesc.Usage       = RHI::BufferUsage::Index;
        IBDesc.Memory      = RHI::MemoryUsage::GpuOnly;
        IBDesc.InitialData = UseU16 ? CAST<const void*>(Indices16.data()) : CAST<const void*>(Indices.data());
        IBDesc.DebugName   = "Mesh IB";
        Mesh.IndexBuffer   = _Device->CreateBuffer(IBDesc);

        if (!Mesh.VertexBuffer.IsValid() || !Mesh.IndexBuffer.IsValid()) {
            THROW_ENGINE_EXCEPTION(EngineException, std::format("GPU mesh buffer creation failed for asset {}", ID.Value));
        }

        return Mesh;
    }

    MeshHandle MeshCache::Acquire(const AssetID ID) {
        if (!ID.IsValid()) return {};

        if (const auto It = _Entries.find(ID.Value); It != _Entries.end()) {
            ++It->second.RefCount;
            return It->second.Handle;
        }

        GpuMesh Mesh {CreateGpuMesh(ID)};
        const MeshHandle Handle {_NextHandleID++};

        _MeshesByHandle.emplace(Handle.ID, std::move(Mesh));
        _Entries.emplace(ID.Value, Entry {Handle, 1});
        return Handle;
    }

    void MeshCache::Release(const AssetID ID) {
        const auto It = _Entries.find(ID.Value);
        if (It == _Entries.end()) return;

        // Guard against an extra Release: underflowing an unsigned count
        // would wrap to ~4 billion and pin the mesh forever.
        if (It->second.RefCount > 0) --It->second.RefCount;
        if (It->second.RefCount > 0) return;

        FreeMesh(It->second.Handle);
        _Entries.erase(It);
    }

    void MeshCache::Preload(const AssetID ID) {
        if (!ID.IsValid() || _Entries.contains(ID.Value)) return;

        GpuMesh Mesh {CreateGpuMesh(ID)};
        const MeshHandle Handle {_NextHandleID++};

        _MeshesByHandle.emplace(Handle.ID, std::move(Mesh));
        _Entries.emplace(ID.Value, Entry {Handle, 0});
    }

    bool MeshCache::IsResident(const AssetID ID) const {
        return _Entries.contains(ID.Value);
    }

    MeshInfo MeshCache::GetInfo(const MeshHandle Handle) const {
        const auto It = _MeshesByHandle.find(Handle.ID);
        return It != _MeshesByHandle.end() ? It->second.Info : MeshInfo {};
    }

    RHI::BufferHandle MeshCache::GetVertexBuffer(const MeshHandle Handle) const {
        const auto It = _MeshesByHandle.find(Handle.ID);
        return It != _MeshesByHandle.end() ? It->second.VertexBuffer : RHI::BufferHandle {};
    }

    RHI::BufferHandle MeshCache::GetIndexBuffer(const MeshHandle Handle) const {
        const auto It = _MeshesByHandle.find(Handle.ID);
        return It != _MeshesByHandle.end() ? It->second.IndexBuffer : RHI::BufferHandle {};
    }

    u32 MeshCache::GetRefCount(const AssetID ID) const {
        const auto It = _Entries.find(ID.Value);
        return It != _Entries.end() ? It->second.RefCount : 0;
    }

    void MeshCache::FreeMesh(const MeshHandle Handle) {
        const auto It = _MeshesByHandle.find(Handle.ID);
        if (It == _MeshesByHandle.end()) return;

        // Safe to call even if this mesh was drawn with this frame: the
        // device defers the GPU delete past every in-flight frame.
        if (_Device) {
            _Device->DestroyBuffer(It->second.VertexBuffer);
            _Device->DestroyBuffer(It->second.IndexBuffer);
        }

        _MeshesByHandle.erase(It);
    }

    void MeshCache::Clear() {
        for (const Entry& E : _Entries | std::views::values) {
            FreeMesh(E.Handle);
        }

        _Entries.clear();
        _MeshesByHandle.clear();
    }
}  // namespace Xen
