//
// Created by Jake Rieger on 9/24/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Component.hpp"
#include "ComponentRegistry.hpp"

#include <algorithm>

namespace Xen {
    REGISTER_COMPONENT(SpotLightComponent)

    /// @brief A cone-shaped light radiating from the owning actor's world
    /// position toward its world-rotated direction - same derivation as
    /// DirectionalLightComponent::GetDirection() (unrotated points down -Z),
    /// just also falling off with distance (see PointLightComponent::Range)
    /// the way a directional light doesn't. InnerConeAngle/OuterConeAngle
    /// bound a smooth angular falloff (PBR.hlsl's SpotAttenuation): fully lit
    /// inside the inner cone, smoothly to zero at the outer one, rather than
    /// a hard-edged circle of light.
    class SpotLightComponent final : public IComponent {
    public:
        XEN_COMPONENT_TYPE(SpotLightComponent)
        SpotLightComponent() = default;

        void Reflect(IReflector& R) override;

        NODISCARD const Float3& GetColor() const { return _Color; }
        void SetColor(const Float3& Color) { _Color = Color; }

        NODISCARD f32 GetIntensity() const { return _Intensity; }
        void SetIntensity(const f32 Intensity) { _Intensity = std::max(Intensity, 0.0f); }

        NODISCARD f32 GetRange() const { return _Range; }
        void SetRange(const f32 Range) { _Range = std::max(Range, 0.01f); }

        /// @brief Half-angle, in degrees, of the fully-lit inner cone.
        /// Clamped below OuterConeAngle - an inner cone wider than the outer
        /// one would make the falloff run backwards.
        NODISCARD f32 GetInnerConeAngle() const { return _InnerConeAngle; }
        void SetInnerConeAngle(const f32 Degrees) { _InnerConeAngle = std::clamp(Degrees, 0.0f, _OuterConeAngle); }

        /// @brief Half-angle, in degrees, beyond which the light contributes
        /// nothing at all.
        NODISCARD f32 GetOuterConeAngle() const { return _OuterConeAngle; }
        void SetOuterConeAngle(const f32 Degrees) {
            _OuterConeAngle  = std::clamp(Degrees, 0.1f, 89.9f);
            _InnerConeAngle  = std::min(_InnerConeAngle, _OuterConeAngle);
        }

        /// @brief World-space direction the light points, derived from the
        /// owning actor's world rotation - identical derivation to
        /// DirectionalLightComponent::GetDirection(), see its own comment.
        NODISCARD Float3 GetDirection() const;

    private:
        Float3 _Color {1.0f, 1.0f, 1.0f};
        f32 _Intensity {1.0f};
        f32 _Range {10.0f};
        f32 _InnerConeAngle {25.0f};
        f32 _OuterConeAngle {35.0f};
    };
}  // namespace Xen
