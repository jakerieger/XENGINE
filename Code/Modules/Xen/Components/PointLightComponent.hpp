//
// Created by Jake Rieger on 9/24/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Component.hpp"
#include "ComponentRegistry.hpp"

#include <algorithm>

namespace Xen {
    REGISTER_COMPONENT(PointLightComponent)

    /// @brief An omnidirectional light radiating from the owning actor's
    /// world position (Transform::Position - unlike DirectionalLightComponent's
    /// direction, position needs no per-frame derivation, so this component
    /// has no GetPosition() wrapper of its own; MeshRenderer reads the
    /// owning Actor's own world transform directly when it collects every
    /// point light in the scene each frame, the same "first read the actor,
    /// then the component" shape MeshComponent/PBRMaterialComponent already
    /// use). Falls off smoothly to zero at Range (see PBR.hlsl's
    /// DistanceAttenuation) rather than a hard, popping cutoff.
    class PointLightComponent final : public IComponent {
    public:
        XEN_COMPONENT_TYPE(PointLightComponent)
        PointLightComponent() = default;

        void Reflect(IReflector& R) override;

        NODISCARD const Float3& GetColor() const { return _Color; }
        void SetColor(const Float3& Color) { _Color = Color; }

        NODISCARD f32 GetIntensity() const { return _Intensity; }
        void SetIntensity(const f32 Intensity) { _Intensity = std::max(Intensity, 0.0f); }

        /// @brief World-space distance at which the light's contribution
        /// reaches zero.
        NODISCARD f32 GetRange() const { return _Range; }
        void SetRange(const f32 Range) { _Range = std::max(Range, 0.01f); }

    private:
        Float3 _Color {1.0f, 1.0f, 1.0f};
        f32 _Intensity {1.0f};
        f32 _Range {10.0f};
    };
}  // namespace Xen
