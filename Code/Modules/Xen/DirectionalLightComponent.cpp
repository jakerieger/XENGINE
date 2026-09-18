//
// Created by Jake Rieger on 9/17/2026.
//

#include "DirectionalLightComponent.hpp"
#include "Actor.hpp"

namespace Xen {
    void DirectionalLightComponent::Reflect(IReflector& R) {
        R.Property("Color", _Color, {.Category = "Light"});
        R.Property("Intensity", _Intensity, {.Category = "Light", .Min = 0.0f});
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
