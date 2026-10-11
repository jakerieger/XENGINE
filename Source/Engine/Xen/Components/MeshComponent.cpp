//
// Created by Jake Rieger on 9/17/2026.
//

#include "MeshComponent.hpp"
#include <Xen/Actor.hpp>
#include <Xen/Scene.hpp>

#include <cstring>

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

    bool MeshComponent::SetAssetProperty(const char* Name, const AssetID ID) {
        if (std::strcmp(Name, "MeshAsset") != 0) return false;
        SetMeshAsset(ID);
        return true;
    }

    void MeshComponent::SetMeshAsset(const AssetID ID) {
        if (ID == _MeshAsset) return;

        Scene* S         = GetScene();
        MeshCache* Cache = S ? S->GetContext().Meshes : nullptr;

        // Acquire the new mesh before releasing the old one, like the texture
        // setters. Acquiring uploads synchronously, which signals the device
        // fence; an old mesh released first would be retired against a fence
        // value that upload then completes, freeing its buffers while this
        // frame's already-recorded draws still reference them.
        const MeshHandle New = (Cache && ID.IsValid()) ? Cache->Acquire(ID) : MeshHandle {};
        if (Cache && _MeshAsset.IsValid()) Cache->Release(_MeshAsset);

        _MeshAsset = ID;
        _Mesh      = New;
    }
}  // namespace Xen
