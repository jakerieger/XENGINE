//
// Created by Jake Rieger on 9/19/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Component.hpp"
#include "ComponentRegistry.hpp"
#include "TextureCache.hpp"

namespace Xen {
    REGISTER_COMPONENT(EnvironmentComponent)

    /// @brief The scene's image-based-lighting source: one equirectangular
    /// environment map (a Radiance .hdr - see TextureCache, which uploads it
    /// as RGBA16F) that MeshRenderer samples by direction for indirect
    /// diffuse and specular lighting.
    ///
    /// Scene-wide, not per-actor - MeshRenderer uses the first
    /// EnvironmentComponent it finds, the same way it picks the first
    /// DirectionalLightComponent. A scene with none (or one with no map
    /// assigned) falls back to a dim two-tone placeholder sky, so an
    /// unlit-by-environment scene still renders as something.
    class EnvironmentComponent final : public IComponent {
    public:
        XEN_COMPONENT_TYPE(EnvironmentComponent)
        EnvironmentComponent() = default;

        void Reflect(IReflector& R) override;

        void BeginPlay() override;
        void EndPlay() override;

        NODISCARD AssetID GetMapAsset() const { return _MapAsset; }
        void SetMapAsset(AssetID ID);

        /// @brief Whether MeshRenderer also draws the environment as the
        /// scene's background, behind everything (on by default). Off keeps
        /// the environment lighting the scene without being visible.
        NODISCARD bool GetShowBackground() const { return _ShowBackground; }
        void SetShowBackground(const bool Show) { _ShowBackground = Show; }

        /// @brief The resolved GPU texture, invalid until BeginPlay (or if
        /// no map is assigned) - MeshRenderer substitutes its placeholder.
        NODISCARD TextureHandle GetMap() const { return _Map; }

    private:
        void Acquire();
        void Release();

        AssetID _MapAsset {};
        bool _ShowBackground {true};
        TextureHandle _Map {};
        bool _Acquired {false};

        // Whether BeginPlay has run - _Acquired can legitimately stay false
        // through it (no map assigned), so it can't double as this signal
        // (same reasoning as PBRMaterialComponent's _Began).
        bool _Began {false};
    };
}  // namespace Xen
