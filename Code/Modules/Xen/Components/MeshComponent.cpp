//
// Created by Jake Rieger on 9/17/2026.
//

#include "MeshComponent.hpp"
#include "Actor.hpp"
#include "Scene.hpp"

namespace Xen {
    MeshComponent::MeshComponent(const AssetID Mesh) : _MeshAsset(Mesh) {}

    void MeshComponent::Reflect(IReflector& R) {
        R.Property("MeshAsset", _MeshAsset, {.Category = "Mesh", .Asset = AssetKind::Mesh});
        // _Mesh is deliberately not reflected: it is a runtime GPU handle,
        // meaningless across sessions, and rebuilt in BeginPlay.
    }

    void MeshComponent::BeginPlay() {
        if (const Scene* S = GetScene(); S && S->GetContext().Meshes && _MeshAsset.IsValid()) {
            _Mesh = S->GetContext().Meshes->Acquire(_MeshAsset);
        }
    }

    void MeshComponent::EndPlay() {
        if (const Scene* S = GetScene(); S && S->GetContext().Meshes && _MeshAsset.IsValid()) {
            S->GetContext().Meshes->Release(_MeshAsset);
        }
        _Mesh = {};
    }

    void MeshComponent::SetMeshAsset(const AssetID ID) {
        Scene* S       = GetScene();
        MeshCache* Cache = S ? S->GetContext().Meshes : nullptr;

        if (Cache && _MeshAsset.IsValid()) Cache->Release(_MeshAsset);

        _MeshAsset = ID;
        _Mesh      = (Cache && ID.IsValid()) ? Cache->Acquire(ID) : MeshHandle {};
    }
}  // namespace Xen
