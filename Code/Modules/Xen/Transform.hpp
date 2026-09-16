//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Math.hpp>

namespace Xen {
    struct Transform {
        Float2 Position {0.0f, 0.0f};
        f32 Rotation {0.0f};  // radians, counter-clockwise
        Float2 Scale {1.0f, 1.0f};

        /// @brief Composes this transform with a parent's, giving a world
        /// transform. Used when actors are attached to one another.
        ///
        /// Decomposed (position/rotation/scale) rather than matrix-based
        /// because the components stay directly inspectable and editable -
        /// extracting rotation back out of a matrix is lossy under non-uniform
        /// scale, and a 2D editor wants to show these as three fields.
        NODISCARD Transform ComposedWith(const Transform& Parent) const {
            const f32 C = std::cos(Parent.Rotation);
            const f32 S = std::sin(Parent.Rotation);

            const Float2 Scaled = Position * Parent.Scale;
            const Float2 Rotated {Scaled.x * C - Scaled.y * S, Scaled.x * S + Scaled.y * C};

            return Transform {
              .Position = Parent.Position + Rotated,
              .Rotation = Parent.Rotation + Rotation,
              .Scale    = Parent.Scale * Scale,
            };
        }

        /// @brief Model matrix for rendering. 4x4 rather than 3x3 so it feeds
        /// a standard vertex shader without conversion.
        ///
        /// Built scale-then-rotate-then-translate for DirectXMath's row-vector
        /// convention (v' = v * M), which is the same TRS ordering glm's
        /// column-vector translate/rotate/scale chain produced.
        NODISCARD Float4x4 ToMatrix() const {
            using namespace DirectX;
            XMMATRIX M = XMMatrixScaling(Scale.x, Scale.y, 1.0f);
            M          = M * XMMatrixRotationZ(Rotation);
            M          = M * XMMatrixTranslation(Position.x, Position.y, 0.0f);

            Float4x4 Out;
            XMStoreFloat4x4(&Out, M);
            return Out;
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
        NODISCARD constexpr bool IsEmpty() const { return Width <= 0.0f || Height <= 0.0f; }

        NODISCARD constexpr f32 Left() const { return X; }
        NODISCARD constexpr f32 Right() const { return X + Width; }
        NODISCARD constexpr f32 Bottom() const { return Y; }
        NODISCARD constexpr f32 Top() const { return Y + Height; }

        /// @brief Overlap test, used for view culling.
        NODISCARD constexpr bool Intersects(const Rect& R) const {
            return !(R.Left() > Right() || R.Right() < Left() || R.Bottom() > Top() || R.Top() < Bottom());
        }

        constexpr bool operator==(const Rect& R) const {
            return X == R.X && Y == R.Y && Width == R.Width && Height == R.Height;
        }
    };
}  // namespace Xen