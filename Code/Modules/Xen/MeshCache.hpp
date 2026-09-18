//
// Created by Jake Rieger on 9/17/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "RenderDevice.hpp"

#include <XenPAK/AssetID.hpp>

#include <unordered_map>

namespace Xen {
    namespace PAK {
        class AssetRegistry;
    }

    /// @brief Opaque ID into MeshCache. Unlike TextureHandle (which just is
    /// the RHI texture resource's own handle - a texture is one GPU
    /// resource), a mesh is a vertex buffer and an index buffer together, so
    /// MeshCache needs its own handle rather than aliasing an RHI one.
    struct MeshHandle {
        u32 ID {0};

        constexpr MeshHandle() = default;
        constexpr explicit MeshHandle(const u32 InID) : ID(InID) {}

        NODISCARD constexpr bool IsValid() const { return ID != 0; }
        constexpr bool operator==(const MeshHandle& Other) const { return ID == Other.ID; }
    };

    struct MeshInfo {
        u32 VertexCount {0};
        u32 IndexCount {0};
        RHI::IndexType IndexType {RHI::IndexType::U32};
    };

    /// @brief Loads .xmesh assets (see MeshAsset.hpp) onto the GPU and caches
    /// them by AssetID, ref-counted - the same acquire/release/preload shape
    /// as TextureCache, so a mesh shared by many actors (a cube instanced a
    /// hundred times) uploads once.
    class MeshCache {
    public:
        explicit MeshCache(PAK::AssetRegistry& Assets, RHI::IRenderDevice& Device) : _Assets(&Assets), _Device(&Device) {}
        ~MeshCache();

        MeshCache(const MeshCache&)            = delete;
        MeshCache& operator=(const MeshCache&) = delete;

        MeshHandle Acquire(AssetID ID);
        void Release(AssetID ID);
        void Preload(AssetID ID);

        NODISCARD bool IsResident(AssetID ID) const;
        NODISCARD MeshInfo GetInfo(MeshHandle Handle) const;
        NODISCARD RHI::BufferHandle GetVertexBuffer(MeshHandle Handle) const;
        NODISCARD RHI::BufferHandle GetIndexBuffer(MeshHandle Handle) const;
        NODISCARD size_t GetResidentCount() const { return _Entries.size(); }
        NODISCARD u32 GetRefCount(AssetID ID) const;

        void Clear();

    private:
        struct GpuMesh {
            RHI::BufferHandle VertexBuffer {};
            RHI::BufferHandle IndexBuffer {};
            MeshInfo Info {};
        };

        struct Entry {
            MeshHandle Handle {};
            u32 RefCount {0};
        };

        GpuMesh CreateGpuMesh(AssetID ID) const;
        void FreeMesh(MeshHandle Handle);

        PAK::AssetRegistry* _Assets {nullptr};
        RHI::IRenderDevice* _Device {nullptr};

        std::unordered_map<PAK::AssetIDValue, Entry> _Entries;
        std::unordered_map<u32, GpuMesh> _MeshesByHandle;
        u32 _NextHandleID {1};  // 0 reserved for "invalid"
    };
}  // namespace Xen
