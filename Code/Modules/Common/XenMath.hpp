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

    constexpr f32 PI      = DirectX::XM_PI;
    constexpr f32 TWO_PI  = DirectX::XM_2PI;
    constexpr f32 HALF_PI = DirectX::XM_PIDIV2;

    /// @brief 4x4 identity, for default member initializers (XMFLOAT4X4 has no
    /// implicit identity - it's a plain POD with an uninitialized default ctor).
    inline const Float4x4 IdentityFloat4x4 {
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

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

    // --- Float4 (colors/tints - only equality is needed today) --------------
    constexpr bool operator==(const Float4& A, const Float4& B) {
        return A.x == B.x && A.y == B.y && A.z == B.z && A.w == B.w;
    }
    constexpr bool operator!=(const Float4& A, const Float4& B) {
        return !(A == B);
    }
}  // namespace Xen
