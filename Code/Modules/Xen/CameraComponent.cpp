//
// Created by Jake Rieger on 9/9/2026.
//

#include "CameraComponent.hpp"
#include "Actor.hpp"

namespace Xen {
    Float4x4 CameraComponent::GetViewMatrix() const {
        using namespace DirectX;
        const Transform T = GetCameraTransform();

        if (_ProjectionMode == ProjectionMode::Perspective) {
            // Full 3D view: the camera's complete orientation, not just a Z
            // angle - a perspective camera can pitch/yaw/roll. +Z/+Y are this
            // engine's canonical forward/up (see DirectionalLightComponent).
            const XMVECTOR Pos     = XMLoadFloat3(&T.Position);
            const XMVECTOR Rot     = XMLoadFloat4(&T.Rotation);
            const XMVECTOR Forward = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), Rot);
            const XMVECTOR Up      = XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), Rot);
            const XMMATRIX View    = XMMatrixLookToLH(Pos, Forward, Up);

            Float4x4 Out;
            XMStoreFloat4x4(&Out, View);
            return Out;
        }

        // Orthographic: built as the inverse of the camera's transform,
        // applied in reverse order: undo rotation, then undo translation
        // ("first apply A, then apply B" is A * B under DirectXMath's
        // row-vector convention). Analytic inverse rather than
        // XMMatrixInverse on the composed transform - cheaper, and exact
        // rather than numerically inverted. Z-axis-only: this mode is still
        // strictly 2D.
        XMMATRIX View = XMMatrixRotationZ(-T.GetRotationZ());
        View          = View * XMMatrixTranslation(-T.Position.x, -T.Position.y, -T.Position.z);

        Float4x4 Out;
        XMStoreFloat4x4(&Out, View);
        return Out;
    }

    Float4x4 CameraComponent::GetProjectionMatrix() const {
        using namespace DirectX;

        if (_ProjectionMode == ProjectionMode::Perspective) {
            const XMMATRIX Proj =
              XMMatrixPerspectiveFovLH(XMConvertToRadians(_FieldOfViewDegrees), GetAspectRatio(), _NearPlane, _FarPlane);

            Float4x4 Out;
            XMStoreFloat4x4(&Out, Proj);
            return Out;
        }

        const Float2 Half = GetVisibleWorldSize() * 0.5f;
        // Y up, matching the world convention.
        const XMMATRIX Proj = XMMatrixOrthographicOffCenterLH(-Half.x, Half.x, -Half.y, Half.y, -1.0f, 1.0f);

        Float4x4 Out;
        XMStoreFloat4x4(&Out, Proj);
        return Out;
    }

    Float2 CameraComponent::GetVisibleWorldSize() const {
        const f32 UnitsPerPixel = 1.0f / (_PixelsPerUnit * _Zoom);
        return Float2 {CAST<f32>(_ViewportWidth) * UnitsPerPixel, CAST<f32>(_ViewportHeight) * UnitsPerPixel};
    }

    Rect CameraComponent::GetViewBounds() const {
        const Transform T   = GetCameraTransform();
        const Float2 Size   = GetVisibleWorldSize();
        const f32 RotZ      = T.GetRotationZ();

        if (RotZ == 0.0f) {
            return Rect {
              T.Position.x - Size.x * 0.5f,
              T.Position.y - Size.y * 0.5f,
              Size.x,
              Size.y,
            };
        }

        const f32 C = std::abs(std::cos(RotZ));
        const f32 S = std::abs(std::sin(RotZ));
        const Float2 Extent {Size.x * C + Size.y * S, Size.x * S + Size.y * C};

        return Rect {
          T.Position.x - Extent.x * 0.5f,
          T.Position.y - Extent.y * 0.5f,
          Extent.x,
          Extent.y,
        };
    }

    Float2 CameraComponent::ScreenToWorld(const Float2 ScreenPos) const {
        const Transform T = GetCameraTransform();
        const Float2 Size = GetVisibleWorldSize();
        const f32 RotZ    = T.GetRotationZ();

        // Screen origin is top-left with y down; world is y up, hence the flip
        // on the y term.
        const Float2 Ndc {ScreenPos.x / CAST<f32>(_ViewportWidth) * 2.0f - 1.0f,
                         1.0f - ScreenPos.y / CAST<f32>(_ViewportHeight) * 2.0f};

        Float2 ViewSpace {Ndc.x * Size.x * 0.5f, Ndc.y * Size.y * 0.5f};

        if (RotZ != 0.0f) {
            const f32 C = std::cos(RotZ);
            const f32 S = std::sin(RotZ);
            ViewSpace   = Float2 {ViewSpace.x * C - ViewSpace.y * S, ViewSpace.x * S + ViewSpace.y * C};
        }

        return Float2 {T.Position.x + ViewSpace.x, T.Position.y + ViewSpace.y};
    }

    Float2 CameraComponent::WorldToScreen(const Float2 WorldPos) const {
        const Transform T = GetCameraTransform();
        const Float2 Size = GetVisibleWorldSize();
        const f32 RotZ    = T.GetRotationZ();

        Float2 Relative {WorldPos.x - T.Position.x, WorldPos.y - T.Position.y};

        if (RotZ != 0.0f) {
            const f32 C = std::cos(-RotZ);
            const f32 S = std::sin(-RotZ);
            Relative    = Float2 {Relative.x * C - Relative.y * S, Relative.x * S + Relative.y * C};
        }

        const Float2 Ndc {Relative.x / (Size.x * 0.5f), Relative.y / (Size.y * 0.5f)};

        return Float2 {(Ndc.x + 1.0f) * 0.5f * CAST<f32>(_ViewportWidth),
                      (1.0f - Ndc.y) * 0.5f * CAST<f32>(_ViewportHeight)};
    }

    Transform CameraComponent::GetCameraTransform() const {
        return GetOwner() ? GetOwner()->GetWorldTransform() : Transform {};
    }
}  // namespace Xen
