//
// Created by Jake Rieger on 9/19/2026.
//

#include "EnvironmentComponent.hpp"
#include "Scene.hpp"

namespace Xen {
    void EnvironmentComponent::Reflect(IReflector& R) {
        R.Property("Map", _MapAsset, {.Category = "Environment", .Asset = AssetKind::Texture});
        R.Property("ShowBackground", _ShowBackground, {.Category = "Environment"});
        // _Map is a runtime GPU handle - meaningless across sessions, rebuilt
        // in BeginPlay, so it's deliberately not reflected.
    }

    void EnvironmentComponent::BeginPlay() {
        Acquire();
        _Began = true;
    }

    void EnvironmentComponent::EndPlay() {
        _Began = false;
        Release();
    }

    void EnvironmentComponent::SetMapAsset(const AssetID ID) {
        if (ID == _MapAsset) return;

        // Not playing yet: BeginPlay's Acquire() will pick this up itself.
        if (!_Began) {
            _MapAsset = ID;
            return;
        }

        Release();
        _MapAsset = ID;
        Acquire();
    }

    void EnvironmentComponent::Acquire() {
        if (!_MapAsset.IsValid()) return;

        Scene* S = GetScene();
        if (!S || !S->GetContext().Textures) return;

        _Map      = S->GetContext().Textures->Acquire(_MapAsset);
        _Acquired = true;
    }

    void EnvironmentComponent::Release() {
        if (!_Acquired) return;

        if (Scene* S = GetScene(); S && S->GetContext().Textures) { S->GetContext().Textures->Release(_MapAsset); }

        _Acquired = false;
        _Map      = {};
    }
}  // namespace Xen
