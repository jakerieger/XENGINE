//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Math.hpp>

namespace Xen {
    /// @brief A full 3D position/rotation/scale. 2D is not a separate code
    /// path: a sprite is a Transform whose Position.z and X/Y rotation stay
    /// zero, viewed through an orthographic camera - the same way Unity,
    /// Godot and Unreal's 2D modes are actually built on their 3D transform
    /// underneath. GetRotationZ/SetRotationZ exist for exactly that 2D case,
    /// so gameplay code that only ever thinks in one rotation angle (every
    /// XenPong component) doesn't need to touch quaternions directly.
    struct Transform {
        Float3 Position {0.0f, 0.0f, 0.0f};
        Quat Rotation {IdentityQuat};
        Float3 Scale {1.0f, 1.0f, 1.0f};

        /// @brief Reads back the Z-axis angle of a rotation that's assumed
        /// pure-Z (true for every 2D actor, since 2D code never sets X/Y
        /// rotation) - meaningless once anything sets a non-Z rotation.
        NODISCARD f32 GetRotationZ() const { return 2.0f * std::atan2(Rotation.z, Rotation.w); }

        /// @brief Sets Rotation to a pure Z-axis rotation of Radians,
        /// discarding any existing X/Y rotation - the 2D convenience that
        /// replaces the old single-float Rotation this superseded.
        void SetRotationZ(const f32 Radians) {
            const f32 Half = Radians * 0.5f;
            Rotation       = {0.0f, 0.0f, std::sin(Half), std::cos(Half)};
        }

        /// @brief Composes this transform with a parent's, giving a world
        /// transform. Used when actors are attached to one another.
        ///
        /// Decomposed (position/rotation/scale) rather than matrix-based
        /// because the components stay directly inspectable and editable -
        /// extracting rotation back out of a matrix is lossy under
        /// non-uniform scale, and an editor wants to show these as fields.
        NODISCARD Transform ComposedWith(const Transform& Parent) const {
            using namespace DirectX;

            const XMVECTOR ParentPos   = XMLoadFloat3(&Parent.Position);
            const XMVECTOR ParentRot   = XMLoadFloat4(&Parent.Rotation);
            const XMVECTOR ParentScale = XMLoadFloat3(&Parent.Scale);

            const XMVECTOR LocalPos   = XMLoadFloat3(&Position);
            const XMVECTOR LocalRot   = XMLoadFloat4(&Rotation);
            const XMVECTOR LocalScale = XMLoadFloat3(&Scale);

            // Scale, then rotate, into the parent's local space before
            // translating by the parent's own world position - the standard
            // scenegraph composition order. XMVectorAdd/Multiply rather than
            // +/* : XMVECTOR has no operator overloads here, so +/* would
            // otherwise silently resolve to this file's own Float2/Float3
            // operators instead (a hard compile error, not a silent bug -
            // but worth knowing why these calls look more verbose than the
            // Float3 arithmetic used everywhere else in this file).
            const XMVECTOR RotatedPos = XMVector3Rotate(XMVectorMultiply(LocalPos, ParentScale), ParentRot);

            Transform Out;
            XMStoreFloat3(&Out.Position, XMVectorAdd(ParentPos, RotatedPos));
            XMStoreFloat4(&Out.Rotation, XMQuaternionMultiply(LocalRot, ParentRot));
            XMStoreFloat3(&Out.Scale, XMVectorMultiply(ParentScale, LocalScale));
            return Out;
        }

        /// @brief Model matrix for rendering. 4x4 rather than 3x3 so it feeds
        /// a standard vertex shader without conversion.
        ///
        /// Built scale-then-rotate-then-translate for DirectXMath's row-vector
        /// convention (v' = v * M), which is the same TRS ordering glm's
        /// column-vector translate/rotate/scale chain produced.
        NODISCARD Float4x4 ToMatrix() const {
            using namespace DirectX;
            XMMATRIX M = XMMatrixScaling(Scale.x, Scale.y, Scale.z);
            M          = M * XMMatrixRotationQuaternion(XMLoadFloat4(&Rotation));
            M          = M * XMMatrixTranslation(Position.x, Position.y, Position.z);

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