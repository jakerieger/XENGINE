//
// Created by Jake Rieger on 9/9/2026.
//

#include "CameraComponent.hpp"
#include "Actor.hpp"

namespace Xen {
    Float4x4 CameraComponent::GetViewMatrix() const {
        using namespace DirectX;
        const Transform T = GetCameraTransform();

        // Built as the inverse of the camera's transform, applied in reverse
        // order: undo rotation, then undo translation ("first apply A, then
        // apply B" is A * B under DirectXMath's row-vector convention).
        // Analytic inverse rather than XMMatrixInverse on the composed
        // transform - cheaper, and exact rather than numerically inverted.
        XMMATRIX View = XMMatrixRotationZ(-T.Rotation);
        View          = View * XMMatrixTranslation(-T.Position.x, -T.Position.y, 0.0f);

        Float4x4 Out;
        XMStoreFloat4x4(&Out, View);
        return Out;
    }

    Float4x4 CameraComponent::GetProjectionMatrix() const {
        using namespace DirectX;
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

        if (T.Rotation == 0.0f) {
            return Rect {
              T.Position.x - Size.x * 0.5f,
              T.Position.y - Size.y * 0.5f,
              Size.x,
              Size.y,
            };
        }

        const f32 C = std::abs(std::cos(T.Rotation));
        const f32 S = std::abs(std::sin(T.Rotation));
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

        // Screen origin is top-left with y down; world is y up, hence the flip
        // on the y term.
        const Float2 Ndc {ScreenPos.x / CAST<f32>(_ViewportWidth) * 2.0f - 1.0f,
                         1.0f - ScreenPos.y / CAST<f32>(_ViewportHeight) * 2.0f};

        Float2 ViewSpace {Ndc.x * Size.x * 0.5f, Ndc.y * Size.y * 0.5f};

        if (T.Rotation != 0.0f) {
            const f32 C = std::cos(T.Rotation);
            const f32 S = std::sin(T.Rotation);
            ViewSpace   = Float2 {ViewSpace.x * C - ViewSpace.y * S, ViewSpace.x * S + ViewSpace.y * C};
        }

        return T.Position + ViewSpace;
    }

    Float2 CameraComponent::WorldToScreen(const Float2 WorldPos) const {
        const Transform T = GetCameraTransform();
        const Float2 Size = GetVisibleWorldSize();

        Float2 Relative = WorldPos - T.Position;

        if (T.Rotation != 0.0f) {
            const f32 C = std::cos(-T.Rotation);
            const f32 S = std::sin(-T.Rotation);
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
