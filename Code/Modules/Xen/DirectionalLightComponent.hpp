//
// Created by Jake Rieger on 9/17/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Component.hpp"
#include "ComponentRegistry.hpp"

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

    private:
        Float3 _Color {1.0f, 1.0f, 1.0f};
        f32 _Intensity {1.0f};
    };
}  // namespace Xen
