//
// Created by Jake Rieger on 9/17/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Component.hpp"
#include "ComponentRegistry.hpp"
#include "TextureCache.hpp"

#include <algorithm>

namespace Xen {
    REGISTER_COMPONENT(PBRMaterialComponent)

    /// @brief Metallic-roughness PBR material: the scalar factors a Cook-
    /// Torrance BRDF needs, plus five optional texture maps (albedo/normal/
    /// metallic-roughness/ambient-occlusion/emissive) at the standardized
    /// slots MeshRenderer's pipeline binds - see MaterialBindings.hpp and
    /// Code/Shaders/Include/MaterialBindings.hlsli. A texture's sampled
    /// value always multiplies its matching scalar factor (glTF's own
    /// convention, since mesh assets are glTF-native - see MeshCache.cpp),
    /// so leaving a map unassigned and just using the scalar is fully
    /// supported, not a degraded path: MeshRenderer binds a white (or
    /// flat-normal) placeholder texture for any channel with no map, which
    /// multiplies through as the identity.
    class PBRMaterialComponent final : public IComponent {
    public:
        XEN_COMPONENT_TYPE(PBRMaterialComponent)
        PBRMaterialComponent() = default;

        void Reflect(IReflector& R) override;

        void BeginPlay() override;
        void EndPlay() override;

        NODISCARD const Float3& GetAlbedo() const { return _Albedo; }
        void SetAlbedo(const Float3& Albedo) { _Albedo = Albedo; }

        NODISCARD f32 GetMetallic() const { return _Metallic; }
        void SetMetallic(const f32 Metallic) { _Metallic = std::clamp(Metallic, 0.0f, 1.0f); }

        NODISCARD f32 GetRoughness() const { return _Roughness; }
        // Never fully 0: a perfectly smooth GGX distribution divides by a
        // near-zero denominator and the specular highlight degenerates to a
        // single point that aliases badly with no shadow map to soften it.
        void SetRoughness(const f32 Roughness) { _Roughness = std::clamp(Roughness, 0.045f, 1.0f); }

        NODISCARD f32 GetAmbientOcclusion() const { return _AmbientOcclusion; }
        void SetAmbientOcclusion(const f32 AO) { _AmbientOcclusion = std::clamp(AO, 0.0f, 1.0f); }

        NODISCARD const Float3& GetEmissive() const { return _Emissive; }
        void SetEmissive(const Float3& Emissive) { _Emissive = Emissive; }

        NODISCARD AssetID GetAlbedoMapAsset() const { return _AlbedoMap.Asset; }
        void SetAlbedoMapAsset(AssetID ID) { SetChannelAsset(_AlbedoMap, ID); }
        NODISCARD TextureHandle GetAlbedoMap() const { return _AlbedoMap.Handle; }

        NODISCARD AssetID GetNormalMapAsset() const { return _NormalMap.Asset; }
        void SetNormalMapAsset(AssetID ID) { SetChannelAsset(_NormalMap, ID); }
        NODISCARD TextureHandle GetNormalMap() const { return _NormalMap.Handle; }

        NODISCARD AssetID GetMetallicRoughnessMapAsset() const { return _MetallicRoughnessMap.Asset; }
        void SetMetallicRoughnessMapAsset(AssetID ID) { SetChannelAsset(_MetallicRoughnessMap, ID); }
        NODISCARD TextureHandle GetMetallicRoughnessMap() const { return _MetallicRoughnessMap.Handle; }

        NODISCARD AssetID GetAmbientOcclusionMapAsset() const { return _AmbientOcclusionMap.Asset; }
        void SetAmbientOcclusionMapAsset(AssetID ID) { SetChannelAsset(_AmbientOcclusionMap, ID); }
        NODISCARD TextureHandle GetAmbientOcclusionMap() const { return _AmbientOcclusionMap.Handle; }

        NODISCARD AssetID GetEmissiveMapAsset() const { return _EmissiveMap.Asset; }
        void SetEmissiveMapAsset(AssetID ID) { SetChannelAsset(_EmissiveMap, ID); }
        NODISCARD TextureHandle GetEmissiveMap() const { return _EmissiveMap.Handle; }

    private:
        Float3 _Albedo {1.0f, 1.0f, 1.0f};
        f32 _Metallic {0.0f};
        f32 _Roughness {0.5f};
        f32 _AmbientOcclusion {1.0f};
        Float3 _Emissive {0.0f, 0.0f, 0.0f};

        /// @brief One optional texture reference: the authored AssetID plus
        /// its resolved GPU handle, invalid until BeginPlay. Every field
        /// below is optional - MeshRenderer supplies a placeholder for
        /// whichever ones are unset (see the class comment).
        struct TextureChannel {
            AssetID Asset {};
            TextureHandle Handle {};
            bool Acquired {false};
            // Albedo/emissive are authored as perceptual (sRGB-encoded)
            // color, like any other color image, and need the GPU to
            // linearize them on sample before they hit the lighting math in
            // PBR.hlsl. Normal/metallic-roughness/occlusion store raw
            // vector/scalar data - per glTF's own convention - and must NOT
            // be decoded, or they come out wrong. See TextureCache::Acquire.
            bool Srgb {false};
        };

        void AcquireChannel(TextureChannel& Channel);
        void ReleaseChannel(TextureChannel& Channel);
        void SetChannelAsset(TextureChannel& Channel, AssetID ID);

        TextureChannel _AlbedoMap {.Srgb = true};
        TextureChannel _NormalMap;
        TextureChannel _MetallicRoughnessMap;
        TextureChannel _AmbientOcclusionMap;
        TextureChannel _EmissiveMap {.Srgb = true};

        // Every channel is optional, so unlike SpriteComponent's single
        // required texture, a channel's own Acquired flag can legitimately
        // stay false through BeginPlay (an unset map). SetChannelAsset needs
        // a reliable "has BeginPlay run" signal of its own to know whether
        // a newly-assigned asset should be acquired immediately.
        bool _Began {false};
    };
}  // namespace Xen
