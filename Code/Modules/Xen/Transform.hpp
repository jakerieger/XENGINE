//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

namespace Xen {
    struct Transform {
        glm::vec2 Position {0.0f, 0.0f};
        f32 Rotation {0.0f};  // radians, counter-clockwise
        glm::vec2 Scale {1.0f, 1.0f};

        /// @brief Composes this transform with a parent's, giving a world
        /// transform. Used when actors are attached to one another.
        ///
        /// Decomposed (position/rotation/scale) rather than matrix-based
        /// because the components stay directly inspectable and editable -
        /// extracting rotation back out of a matrix is lossy under non-uniform
        /// scale, and a 2D editor wants to show these as three fields.
        _NoDiscard Transform ComposedWith(const Transform& Parent) const {
            const f32 C = std::cos(Parent.Rotation);
            const f32 S = std::sin(Parent.Rotation);

            const glm::vec2 Scaled = Position * Parent.Scale;
            const glm::vec2 Rotated {Scaled.x * C - Scaled.y * S, Scaled.x * S + Scaled.y * C};

            return Transform {
              .Position = Parent.Position + Rotated,
              .Rotation = Parent.Rotation + Rotation,
              .Scale    = Parent.Scale * Scale,
            };
        }

        /// @brief Model matrix for rendering. mat4 rather than mat3 so it
        /// feeds a standard vertex shader without conversion.
        _NoDiscard glm::mat4 ToMatrix() const {
            glm::mat4 M = glm::translate(glm::mat4 {1.0f}, glm::vec3 {Position, 0.0f});
            M           = glm::rotate(M, Rotation, glm::vec3 {0.0f, 0.0f, 1.0f});
            M           = glm::scale(M, glm::vec3 {Scale, 1.0f});
            return M;
        }
    };

    struct Rect {
        f32 X {0.0f};
        f32 Y {0.0f};
        f32 Width {0.0f};
        f32 Height {0.0f};

        constexpr Rect() = default;
        constexpr Rect(const f32 InX, const f32 InY, const f32 W, const f32 H) : X(InX), Y(InY), Width(W), Height(H) {}

        /// @brief A zero-area rect means "no region specified". Sprite code
        /// reads that as "use the whole texture", which is what lets one
        /// component serve both atlas regions and standalone textures.
        _NoDiscard constexpr bool IsEmpty() const { return Width <= 0.0f || Height <= 0.0f; }

        _NoDiscard constexpr f32 Left() const { return X; }
        _NoDiscard constexpr f32 Right() const { return X + Width; }
        _NoDiscard constexpr f32 Bottom() const { return Y; }
        _NoDiscard constexpr f32 Top() const { return Y + Height; }

        /// @brief Overlap test, used for view culling.
        _NoDiscard constexpr bool Intersects(const Rect& R) const {
            return !(R.Left() > Right() || R.Right() < Left() || R.Bottom() > Top() || R.Top() < Bottom());
        }

        constexpr bool operator==(const Rect& R) const {
            return X == R.X && Y == R.Y && Width == R.Width && Height == R.Height;
        }
    };
}  // namespace Xen