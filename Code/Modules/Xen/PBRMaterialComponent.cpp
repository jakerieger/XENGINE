//
// Created by Jake Rieger on 9/17/2026.
//

#include "PBRMaterialComponent.hpp"
#include "Scene.hpp"

namespace Xen {
    void PBRMaterialComponent::Reflect(IReflector& R) {
        R.Property("Albedo", _Albedo, {.Category = "Material"});
        R.Property("Metallic", _Metallic, {.Category = "Material", .Min = 0.0f, .Max = 1.0f});
        R.Property("Roughness", _Roughness, {.Category = "Material", .Min = 0.045f, .Max = 1.0f});
        R.Property("AmbientOcclusion", _AmbientOcclusion, {.Category = "Material", .Min = 0.0f, .Max = 1.0f});
        R.Property("Emissive", _Emissive, {.Category = "Material"});

        R.Property("AlbedoMap", _AlbedoMap.Asset, {.Category = "Material Maps", .Asset = AssetKind::Texture});
        R.Property("NormalMap", _NormalMap.Asset, {.Category = "Material Maps", .Asset = AssetKind::Texture});
        R.Property("MetallicRoughnessMap",
                   _MetallicRoughnessMap.Asset,
                   {.Category = "Material Maps", .Asset = AssetKind::Texture});
        R.Property("AmbientOcclusionMap",
                   _AmbientOcclusionMap.Asset,
                   {.Category = "Material Maps", .Asset = AssetKind::Texture});
        R.Property("EmissiveMap", _EmissiveMap.Asset, {.Category = "Material Maps", .Asset = AssetKind::Texture});
        // The resolved TextureHandle in each channel is deliberately not
        // reflected: it's a runtime GPU handle, meaningless across sessions,
        // and rebuilt in BeginPlay.
    }

    void PBRMaterialComponent::BeginPlay() {
        AcquireChannel(_AlbedoMap);
        AcquireChannel(_NormalMap);
        AcquireChannel(_MetallicRoughnessMap);
        AcquireChannel(_AmbientOcclusionMap);
        AcquireChannel(_EmissiveMap);
        _Began = true;
    }

    void PBRMaterialComponent::EndPlay() {
        _Began = false;

        ReleaseChannel(_AlbedoMap);
        ReleaseChannel(_NormalMap);
        ReleaseChannel(_MetallicRoughnessMap);
        ReleaseChannel(_AmbientOcclusionMap);
        ReleaseChannel(_EmissiveMap);
    }

    void PBRMaterialComponent::AcquireChannel(TextureChannel& Channel) {
        // Unlike SpriteComponent's single required texture, every map here
        // is optional - MeshRenderer falls back to a placeholder, so an
        // unset AssetID just means "no map for this channel," not an error.
        if (!Channel.Asset.IsValid()) return;

        Scene* S = GetScene();
        if (!S || !S->GetContext().Textures) return;

        Channel.Handle   = S->GetContext().Textures->Acquire(Channel.Asset, Channel.Srgb);
        Channel.Acquired = true;
    }

    void PBRMaterialComponent::ReleaseChannel(TextureChannel& Channel) {
        if (!Channel.Acquired) return;

        if (Scene* S = GetScene(); S && S->GetContext().Textures) { S->GetContext().Textures->Release(Channel.Asset); }

        Channel.Acquired = false;
        Channel.Handle   = {};
    }

    void PBRMaterialComponent::SetChannelAsset(TextureChannel& Channel, const AssetID ID) {
        if (ID == Channel.Asset) return;

        // Not playing yet (or a channel whose asset was set before
        // BeginPlay ever ran): AcquireChannel will pick this up itself.
        if (!_Began) {
            Channel.Asset = ID;
            return;
        }

        Scene* S            = GetScene();
        TextureCache* Cache = S ? S->GetContext().Textures : nullptr;
        if (!Cache) {
            Channel.Asset = ID;
            return;
        }

        const TextureHandle New = ID.IsValid() ? Cache->Acquire(ID, Channel.Srgb) : TextureHandle {};
        if (Channel.Acquired) Cache->Release(Channel.Asset);

        Channel.Asset    = ID;
        Channel.Handle   = New;
        Channel.Acquired = ID.IsValid();
    }
}  // namespace Xen
