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

        /// @brief How much a shadowed point's *diffuse* image-based lighting
        /// is darkened, 0..1 - 0 leaves it untouched (physically, ambient
        /// sky light isn't blocked by this light's own shadow ray, so a
        /// shadow only removes DirectLight, which is all the shadow map
        /// alone actually models), 1 removes it entirely in full shadow.
        /// PBR.hlsl only applies this to the diffuse IBL term, never the
        /// specular reflection - a mirror-ish surface legitimately keeps
        /// showing the environment regardless of this light's shadow. A
        /// single low-res dynamic shadow map is already a crude
        /// approximation with no real occlusion/GI behind it, so this exists
        /// as a cheap, deliberately non-physical "contact darkening" knob -
        /// without it, a strong HDRI's ambient contribution alone can keep a
        /// fully shadowed diffuse surface looking nearly as bright as a lit
        /// one, reading as faded/washed-out shadows rather than a real one.
        NODISCARD f32 GetShadowAmbientDarkening() const { return _ShadowAmbientDarkening; }
        void SetShadowAmbientDarkening(const f32 Darkening) { _ShadowAmbientDarkening = std::clamp(Darkening, 0.0f, 1.0f); }

    private:
        Float3 _Color {1.0f, 1.0f, 1.0f};
        f32 _Intensity {1.0f};

        // A single shadow map fitted to the camera's view frustum out to
        // ShadowDistance (see MeshRenderer). Bias values are in shadow-map
        // texels, not world units, so they keep working when the distance or
        // resolution changes.
        //
        // Bias/NormalBias were originally 1.0/1.5 - visibly insufficient
        // even for an ordinary directional light over a flat floor at a
        // moderate (not grazing) angle: the receiver's re-derived light-space
        // depth and the shadow pass's own rasterized depth take genuinely
        // different interpolation paths for the same physical point (main
        // pass -> WorldPosition -> reproject, vs. the shadow pass's own
        // screen-space rasterization), and at the old bias the two were
        // close enough to flip pass/fail back and forth smoothly across the
        // surface - not random noise, so no amount of PCF (filtering *which*
        // nearby texels get read) touched it, but a coherent, distance-
        // varying rippled pattern across the whole receiver. 4.0/3.0 was
        // verified (via a direct Shadow-term-only visualization) to fully
        // clear it on Demo.PBR with no visible peter-panning at the monkey's
        // contact shadow.
        bool _CastShadows {true};
        f32 _ShadowDistance {40.0f};
        u32 _ShadowResolution {2048};
        f32 _ShadowBias {4.0f};
        f32 _ShadowNormalBias {3.0f};
        f32 _ShadowSoftness {1.0f};
        f32 _ShadowAmbientDarkening {0.6f};
    };
}  // namespace Xen
