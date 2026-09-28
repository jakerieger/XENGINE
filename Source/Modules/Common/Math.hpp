//
// Created by Jake Rieger on 9/15/2026.
//

#pragma once

#include "XenCommon.hpp"

#include <DirectXMath.h>

namespace Xen {
    // Plain POD storage types for anything kept as a class member (matches the
    // standard DirectXMath idiom: store XMFLOAT*, XMLoadFloat* into an
    // XMVECTOR/XMMATRIX to compute, XMStoreFloat* back). Kept as bare aliases
    // rather than wrapper classes so layout stays identical to
    // DirectX::XMFLOAT2/3/4/4X4 - required wherever these are mirrored into a
    // GPU-visible struct.
    using Float2   = DirectX::XMFLOAT2;
    using Float3   = DirectX::XMFLOAT3;
    using Float4   = DirectX::XMFLOAT4;
    using Float4x4 = DirectX::XMFLOAT4X4;

    /// @brief Same underlying type as Float4 (a using-alias, not a distinct
    /// type - so no separate IReflector::Visit overload is needed; it
    /// dispatches to Float4's), spelled differently at field/parameter
    /// declarations purely so a Transform::Rotation reads as what it is.
    using Quat = DirectX::XMFLOAT4;

    constexpr f32 PI      = DirectX::XM_PI;
    constexpr f32 TWO_PI  = DirectX::XM_2PI;
    constexpr f32 HALF_PI = DirectX::XM_PIDIV2;

    /// @brief 4x4 identity, for default member initializers (XMFLOAT4X4 has no
    /// implicit identity - it's a plain POD with an uninitialized default ctor).
    inline constexpr Float4x4 IdentityFloat4x4 {
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

    /// @brief No-rotation quaternion, for default member initializers - same
    /// reasoning as IdentityFloat4x4 (a zero-initialized XMFLOAT4 is not a
    /// valid rotation).
    inline constexpr Quat IdentityQuat {0.0f, 0.0f, 0.0f, 1.0f};

    // --- Float2 -------------------------------------------------------------
    // XMFLOAT2/3/4 are plain PODs with no operators of their own - DirectXMath
    // only overloads arithmetic on XMVECTOR. These small helpers keep call
    // sites (Transform composition, gameplay physics) as readable as the old
    // glm::vec2 code without routing every add/scale through a SIMD load/store.

    constexpr Float2 operator+(const Float2& A, const Float2& B) {
        return {A.x + B.x, A.y + B.y};
    }
    constexpr Float2 operator-(const Float2& A, const Float2& B) {
        return {A.x - B.x, A.y - B.y};
    }
    constexpr Float2 operator-(const Float2& A) {
        return {-A.x, -A.y};
    }
    constexpr Float2 operator*(const Float2& A, const Float2& B) {
        return {A.x * B.x, A.y * B.y};
    }
    constexpr Float2 operator*(const Float2& A, const f32 S) {
        return {A.x * S, A.y * S};
    }
    constexpr Float2 operator*(const f32 S, const Float2& A) {
        return A * S;
    }
    constexpr Float2 operator/(const Float2& A, const f32 S) {
        return {A.x / S, A.y / S};
    }
    inline Float2& operator+=(Float2& A, const Float2& B) {
        A = A + B;
        return A;
    }
    inline Float2& operator-=(Float2& A, const Float2& B) {
        A = A - B;
        return A;
    }
    inline Float2& operator*=(Float2& A, const f32 S) {
        A = A * S;
        return A;
    }
    constexpr bool operator==(const Float2& A, const Float2& B) {
        return A.x == B.x && A.y == B.y;
    }
    constexpr bool operator!=(const Float2& A, const Float2& B) {
        return !(A == B);
    }

    // --- Float3 ---------------------------------------------------------------

    constexpr Float3 operator+(const Float3& A, const Float3& B) {
        return {A.x + B.x, A.y + B.y, A.z + B.z};
    }
    constexpr Float3 operator-(const Float3& A, const Float3& B) {
        return {A.x - B.x, A.y - B.y, A.z - B.z};
    }
    constexpr Float3 operator-(const Float3& A) {
        return {-A.x, -A.y, -A.z};
    }
    constexpr Float3 operator*(const Float3& A, const Float3& B) {
        return {A.x * B.x, A.y * B.y, A.z * B.z};
    }
    constexpr Float3 operator*(const Float3& A, const f32 S) {
        return {A.x * S, A.y * S, A.z * S};
    }
    constexpr Float3 operator*(const f32 S, const Float3& A) {
        return A * S;
    }
    constexpr Float3 operator/(const Float3& A, const f32 S) {
        return {A.x / S, A.y / S, A.z / S};
    }
    inline Float3& operator+=(Float3& A, const Float3& B) {
        A = A + B;
        return A;
    }
    inline Float3& operator-=(Float3& A, const Float3& B) {
        A = A - B;
        return A;
    }
    inline Float3& operator*=(Float3& A, const f32 S) {
        A = A * S;
        return A;
    }
    constexpr bool operator==(const Float3& A, const Float3& B) {
        return A.x == B.x && A.y == B.y && A.z == B.z;
    }
    constexpr bool operator!=(const Float3& A, const Float3& B) {
        return !(A == B);
    }

    // --- Float4 (colors/tints - only equality is needed today) --------------
    constexpr bool operator==(const Float4& A, const Float4& B) {
        return A.x == B.x && A.y == B.y && A.z == B.z && A.w == B.w;
    }
    constexpr bool operator!=(const Float4& A, const Float4& B) {
        return !(A == B);
    }

    // --- Helper functions and conversions --------------
    inline Float3 QuaternionToEuler(const Quat& Q) {
        const float XX = Q.x * Q.x;
        const float YY = Q.y * Q.y;
        const float ZZ = Q.z * Q.z;

        const float M31 = 2.f * (Q.x * Q.z + Q.y * Q.w);
        const float M32 = 2.f * (Q.y * Q.z - Q.x * Q.w);
        const float M33 = 1.f - 2.f * (XX + YY);

        const float CosPitch = std::sqrtf(M31 * M31 + M33 * M33);
        const float Pitch    = std::atan2f(-M32, CosPitch);

        if (CosPitch > 16.f * FLT_EPSILON) {
            const float M12 = 2.f * (Q.x * Q.y + Q.z * Q.w);
            const float M22 = 1.f - 2.f * (XX + ZZ);
            return {
              Pitch,
              std::atan2f(M31, M33),
              std::atan2f(M12, M22),
            };
        } else {
            const float M11 = 1.f - 2.f * (YY + ZZ);
            const float M21 = 2.f * (Q.x * Q.y - Q.z * Q.w);
            return {
              Pitch,
              0.f,
              std::atan2f(-M21, M11),
            };
        }
    }

    inline Quat EulerToQuaternion(const Float3& E) {
        Float4 Q;
        DirectX::XMStoreFloat4(&Q, DirectX::XMQuaternionRotationRollPitchYaw(E.x, E.y, E.z));
        return Q;
    }
}  // namespace Xen
