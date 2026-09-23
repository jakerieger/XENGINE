//
// Created by Jake Rieger on 9/17/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Component.hpp"
#include "ComponentRegistry.hpp"
#include "MeshCache.hpp"

namespace Xen {
    REGISTER_COMPONENT(MeshComponent)

    /// @brief References a mesh asset (glTF/GLB - see MeshCache.cpp),
    /// resolving it to GPU buffers via the scene's MeshCache - the 3D
    /// analogue of SpriteComponent's texture reference.
    class MeshComponent final : public IComponent {
    public:
        XEN_COMPONENT_TYPE(MeshComponent)
        MeshComponent() = default;
        explicit MeshComponent(AssetID Mesh);

        void Reflect(IReflector& R) override;
        void BeginPlay() override;
        void EndPlay() override;

        NODISCARD AssetID GetMeshAsset() const { return _MeshAsset; }

        /// @brief Swaps the mesh at runtime, releasing the old one and
        /// acquiring the new so reference counts stay balanced.
        void SetMeshAsset(AssetID ID);

        /// @brief The resolved GPU mesh. Invalid until BeginPlay has run.
        NODISCARD MeshHandle GetMesh() const { return _Mesh; }

    private:
        AssetID _MeshAsset {};

        // Runtime state
        MeshHandle _Mesh {};
    };
}  // namespace Xen
