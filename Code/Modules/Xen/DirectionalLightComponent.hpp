//
// Created by Jake Rieger on 9/17/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Component.hpp"
#include "ComponentRegistry.hpp"

#include <algorithm>

namespace Xen {
    REGISTER_COMPONENT(DirectionalLightComponent)

    /// @brief An infinitely-distant light (sun-like) whose direction comes
    /// from the owning actor's rotation, not a separately-stored vector -
    /// the same reason a directional light is usually authored as a rotated
    /// empty rather than a raw direction: it composes with parenting,
    /// gizmos, and animation for free. Unrotated, it points down -Z (this
    /// engine's canonical forward, matching glTF's convention - see
    /// CameraComponent).
    class DirectionalLightComponent final : public IComponent {
    public:
        XEN_COMPONENT_TYPE(DirectionalLightComponent)
        DirectionalLightComponent() = default;

        void Reflect(IReflector& R) override;

        NODISCARD const Float3& GetColor() const { return _Color; }
        void SetColor(const Float3& Color) { _Color = Color; }

        NODISCARD f32 GetIntensity() const { return _Intensity; }
        void SetIntensity(const f32 Intensity) { _Intensity = Intensity; }

        /// @brief World-space direction the light travels, derived from the
        /// owning actor's world rotation applied to the canonical -Z
        /// "forward" - falls back to {0,0,-1} if called with no owner, which
        /// should only happen before an actor adopts it.
        NODISCARD Float3 GetDirection() const;

        NODISCARD bool GetCastShadows() const { return _CastShadows; }
        void SetCastShadows(const bool Cast) { _CastShadows = Cast; }

        NODISCARD f32 GetShadowDistance() const { return _ShadowDistance; }
        void SetShadowDistance(const f32 Distance) { _ShadowDistance = std::max(Distance, 0.1f); }

        NODISCARD u32 GetShadowResolution() const { return _ShadowResolution; }
        void SetShadowResolution(const u32 Resolution) { _ShadowResolution = std::clamp<u32>(Resolution, 256, 8192); }

        NODISCARD f32 GetShadowBias() const { return _ShadowBias; }
        void SetShadowBias(const f32 Texels) { _ShadowBias = std::max(Texels, 0.0f); }

        NODISCARD f32 GetShadowNormalBias() const { return _ShadowNormalBias; }
        void SetShadowNormalBias(const f32 Texels) { _ShadowNormalBias = std::max(Texels, 0.0f); }

        NODISCARD f32 GetShadowSoftness() const { return _ShadowSoftness; }
        void SetShadowSoftness(const f32 Texels) { _ShadowSoftness = std::max(Texels, 0.0f); }

    private:
        Float3 _Color {1.0f, 1.0f, 1.0f};
        f32 _Intensity {1.0f};

        // A single shadow map fitted to the camera's view frustum out to
        // ShadowDistance (see MeshRenderer). Bias values are in shadow-map
        // texels, not world units, so they keep working when the distance or
        // resolution changes.
        bool _CastShadows {true};
        f32 _ShadowDistance {40.0f};
        u32 _ShadowResolution {2048};
        f32 _ShadowBias {1.0f};
        f32 _ShadowNormalBias {1.5f};
        f32 _ShadowSoftness {1.0f};
    };
}  // namespace Xen
