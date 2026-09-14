//
// Created by Jake Rieger on 9/9/2026.
//

#include "CameraComponent.hpp"
#include "Actor.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace Xen {
    glm::mat4 CameraComponent::GetViewMatrix() const {
        const Transform T = GetCameraTransform();
        // Built as the inverse of the camera's transform, applied in reverse
        // order: undo rotation, then undo translation. Composing forward and
        // calling glm::inverse would work but costs a full 4x4 inversion every
        // frame for a transform we can invert analytically.
        glm::mat4 View = glm::rotate(glm::mat4 {1.0f}, -T.Rotation, glm::vec3 {0.0f, 0.0f, 1.0f});
        View           = glm::translate(View, glm::vec3 {-T.Position, 0.0f});
        return View;
    }

    glm::mat4 CameraComponent::GetProjectionMatrix() const {
        const glm::vec2 Half = GetVisibleWorldSize() * 0.5f;
        // Y up, matching the world convention. A Vulkan backend flips Y in the
        // projection or via a negative viewport height - do it there rather
        // than here, so world space stays consistent across backends.
        return glm::ortho(-Half.x, Half.x, -Half.y, Half.y, -1.0f, 1.0f);
    }

    glm::vec2 CameraComponent::GetVisibleWorldSize() const {
        const f32 UnitsPerPixel = 1.0f / (_PixelsPerUnit * _Zoom);
        return glm::vec2 {CAST<f32>(_ViewportWidth) * UnitsPerPixel, CAST<f32>(_ViewportHeight) * UnitsPerPixel};
    }

    Rect CameraComponent::GetViewBounds() const {
        const Transform T    = GetCameraTransform();
        const glm::vec2 Size = GetVisibleWorldSize();

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
        const glm::vec2 Extent {Size.x * C + Size.y * S, Size.x * S + Size.y * C};

        return Rect {
          T.Position.x - Extent.x * 0.5f,
          T.Position.y - Extent.y * 0.5f,
          Extent.x,
          Extent.y,
        };
    }

    glm::vec2 CameraComponent::ScreenToWorld(const glm::vec2 ScreenPos) const {
        const Transform T    = GetCameraTransform();
        const glm::vec2 Size = GetVisibleWorldSize();

        // Screen origin is top-left with y down; world is y up, hence the flip
        // on the y term.
        const glm::vec2 Ndc {ScreenPos.x / CAST<f32>(_ViewportWidth) * 2.0f - 1.0f,
                             1.0f - ScreenPos.y / CAST<f32>(_ViewportHeight) * 2.0f};

        glm::vec2 ViewSpace {Ndc.x * Size.x * 0.5f, Ndc.y * Size.y * 0.5f};

        if (T.Rotation != 0.0f) {
            const f32 C = std::cos(T.Rotation);
            const f32 S = std::sin(T.Rotation);
            ViewSpace   = glm::vec2 {ViewSpace.x * C - ViewSpace.y * S, ViewSpace.x * S + ViewSpace.y * C};
        }

        return T.Position + ViewSpace;
    }

    glm::vec2 CameraComponent::WorldToScreen(const glm::vec2 WorldPos) const {
        const Transform T    = GetCameraTransform();
        const glm::vec2 Size = GetVisibleWorldSize();

        glm::vec2 Relative = WorldPos - T.Position;

        if (T.Rotation != 0.0f) {
            const f32 C = std::cos(-T.Rotation);
            const f32 S = std::sin(-T.Rotation);
            Relative    = glm::vec2 {Relative.x * C - Relative.y * S, Relative.x * S + Relative.y * C};
        }

        const glm::vec2 Ndc {Relative.x / (Size.x * 0.5f), Relative.y / (Size.y * 0.5f)};

        return glm::vec2 {(Ndc.x + 1.0f) * 0.5f * CAST<f32>(_ViewportWidth),
                          (1.0f - Ndc.y) * 0.5f * CAST<f32>(_ViewportHeight)};
    }

    Transform CameraComponent::GetCameraTransform() const {
        return GetOwner() ? GetOwner()->GetWorldTransform() : Transform {};
    }
}  // namespace Xen