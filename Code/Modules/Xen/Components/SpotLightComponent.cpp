//
// Created by Jake Rieger on 9/24/2026.
//

#include "SpotLightComponent.hpp"
#include "Actor.hpp"

namespace Xen {
    void SpotLightComponent::Reflect(IReflector& R) {
        R.Property("Color", _Color, {.Category = "Light"});
        R.Property("Intensity", _Intensity, {.Category = "Light", .Min = 0.0f});
        R.Property("Range",
                   _Range,
                   {.ToolTip  = "World-space distance at which the light's contribution reaches zero.",
                    .Category = "Light",
                    .Min      = 0.01f});
        R.Property("InnerConeAngle",
                   _InnerConeAngle,
                   {.ToolTip  = "Half-angle, in degrees, of the fully-lit inner cone.",
                    .Category = "Light",
                    .Min      = 0.0f,
                    .Max      = 89.9f});
        R.Property("OuterConeAngle",
                   _OuterConeAngle,
                   {.ToolTip  = "Half-angle, in degrees, beyond which the light contributes nothing.",
                    .Category = "Light",
                    .Min      = 0.1f,
                    .Max      = 89.9f});
    }

    Float3 SpotLightComponent::GetDirection() const {
        if (!GetOwner()) return {0.0f, 0.0f, -1.0f};

        using namespace DirectX;
        const Transform T = GetOwner()->GetWorldTransform();
        // Same derivation as DirectionalLightComponent::GetDirection() - see
        // its own comment for why -Z/rotation, not a stored vector.
        const XMVECTOR Forward        = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
        const XMVECTOR RotatedForward = XMVector3Rotate(Forward, XMLoadFloat4(&T.Rotation));

        Float3 Out;
        XMStoreFloat3(&Out, RotatedForward);
        return Out;
    }
}  // namespace Xen
