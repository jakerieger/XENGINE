//
// Created by Jake Rieger on 9/17/2026.
//

#include "DirectionalLightComponent.hpp"
#include "Actor.hpp"

namespace Xen {
    void DirectionalLightComponent::Reflect(IReflector& R) {
        R.Property("Color", _Color, {.Category = "Light"});
        R.Property("Intensity", _Intensity, {.Category = "Light", .Min = 0.0f});
        R.Property("CastShadows",
                   _CastShadows,
                   {.ToolTip = "Render a shadow map from this light (perspective cameras only).", .Category = "Shadows"});
        R.Property("ShadowDistance",
                   _ShadowDistance,
                   {.ToolTip  = "How far from the camera shadows reach. One shadow map covers this whole range, so "
                                "a shorter distance means sharper shadows.",
                    .Category = "Shadows",
                    .Min      = 0.1f});
        R.Property("ShadowResolution",
                   _ShadowResolution,
                   {.ToolTip = "Shadow map size in texels per side (256-8192).", .Category = "Shadows", .Min = 256.0f, .Max = 8192.0f});
        R.Property("ShadowBias",
                   _ShadowBias,
                   {.ToolTip  = "Constant depth offset, in shadow-map texels. Raise it if surfaces show striped "
                                "self-shadowing (acne); too much detaches shadows from their casters.",
                    .Category = "Shadows",
                    .Min      = 0.0f});
        R.Property("ShadowNormalBias",
                   _ShadowNormalBias,
                   {.ToolTip  = "Offsets the lookup along the surface normal, in shadow-map texels. Fixes acne on "
                                "surfaces facing away from the light without detaching shadows.",
                    .Category = "Shadows",
                    .Min      = 0.0f});
        R.Property("ShadowSoftness",
                   _ShadowSoftness,
                   {.ToolTip  = "Blur radius of the shadow edge, in shadow-map texels.",
                    .Category = "Shadows",
                    .Min      = 0.0f});
    }

    Float3 DirectionalLightComponent::GetDirection() const {
        if (!GetOwner()) return {0.0f, 0.0f, -1.0f};

        using namespace DirectX;
        const Transform T = GetOwner()->GetWorldTransform();
        // -Z is this engine's canonical "forward" (matching the right-handed
        // convention XMMatrixLookToRH/XMMatrixPerspectiveFovRH expect, and
        // glTF's own +Y-up/-Z-forward convention - see
        // CameraComponent::GetViewMatrix's perspective branch), so an
        // unrotated light points the same way an unrotated camera looks.
        const XMVECTOR Forward        = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
        const XMVECTOR RotatedForward = XMVector3Rotate(Forward, XMLoadFloat4(&T.Rotation));

        Float3 Out;
        XMStoreFloat3(&Out, RotatedForward);
        return Out;
    }
}  // namespace Xen
