//
// Created by Jake Rieger on 9/17/2026.
//

#include <Common/Exception.hpp>

#include "MeshCache.hpp"
#include "MeshAsset.hpp"

#include <XenPAK/AssetRegistry.hpp>
#include <XenPAK/AssetBuffer.hpp>

#include <cstring>
#include <format>
#include <ranges>

namespace Xen {
    MeshCache::~MeshCache() {
        Clear();
    }

    MeshCache::GpuMesh MeshCache::CreateGpuMesh(const AssetID ID) const {
        if (!_Assets->Contains(ID)) {
            THROW_ENGINE_EXCEPTION(EngineException, std::format("mesh asset {} not found", ID.Value));
        }

        const PAK::AssetBuffer Encoded = _Assets->Load(ID);
        if (Encoded.Size() < sizeof(MeshAssetHeader)) {
            THROW_ENGINE_EXCEPTION(EngineException, std::format("mesh asset {} is truncated", ID.Value));
        }

        MeshAssetHeader Header;
        std::memcpy(&Header, Encoded.Data(), sizeof(Header));

        if (Header.Magic != MESH_ASSET_MAGIC || Header.Version != MESH_ASSET_VERSION) {
            THROW_ENGINE_EXCEPTION(EngineException, std::format("mesh asset {} has an unrecognized format", ID.Value));
        }
        if (Header.IndexStride != 2 && Header.IndexStride != 4) {
            THROW_ENGINE_EXCEPTION(EngineException, std::format("mesh asset {} has an invalid index stride", ID.Value));
        }

        const u8* VertexData      = Encoded.Data() + sizeof(MeshAssetHeader);
        const size_t VertexBytes  = CAST<size_t>(Header.VertexCount) * sizeof(MeshAssetVertex);
        const u8* IndexData       = VertexData + VertexBytes;
        const size_t IndexBytes   = CAST<size_t>(Header.IndexCount) * Header.IndexStride;

        if (sizeof(MeshAssetHeader) + VertexBytes + IndexBytes > Encoded.Size()) {
            THROW_ENGINE_EXCEPTION(EngineException, std::format("mesh asset {} is truncated", ID.Value));
        }

        GpuMesh Mesh;
        Mesh.Info.VertexCount = Header.VertexCount;
        Mesh.Info.IndexCount  = Header.IndexCount;
        Mesh.Info.IndexType   = Header.IndexStride == 2 ? RHI::IndexType::U16 : RHI::IndexType::U32;

        RHI::BufferDesc VBDesc;
        VBDesc.Size        = VertexBytes;
        VBDesc.Usage       = RHI::BufferUsage::Vertex;
        VBDesc.Memory      = RHI::MemoryUsage::GpuOnly;
        VBDesc.InitialData = VertexData;
        VBDesc.DebugName   = "Mesh VB";
        Mesh.VertexBuffer  = _Device->CreateBuffer(VBDesc);

        RHI::BufferDesc IBDesc;
        IBDesc.Size        = IndexBytes;
        IBDesc.Usage       = RHI::BufferUsage::Index;
        IBDesc.Memory      = RHI::MemoryUsage::GpuOnly;
        IBDesc.InitialData = IndexData;
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
