//
// Created by Jake Rieger on 9/6/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include <string>
#include <functional>

namespace Xen::PAK {
    using AssetIDValue = u64;

    struct AssetID {
        AssetIDValue Value;

        constexpr AssetID() = default;
        constexpr explicit AssetID(const AssetIDValue IDValue) : Value(IDValue) {}

        constexpr bool operator==(const AssetID& Rhs) const { return Value == Rhs.Value; }
        constexpr bool operator!=(const AssetID& Rhs) const { return Value != Rhs.Value; }
        constexpr bool operator<(const AssetID& Rhs) const { return Value < Rhs.Value; }
        constexpr bool operator>(const AssetID& Rhs) const { return Value > Rhs.Value; }
        constexpr bool operator<=(const AssetID& Rhs) const { return Value <= Rhs.Value; }
        constexpr bool operator>=(const AssetID& Rhs) const { return Value >= Rhs.Value; }

        [[nodiscard]] constexpr bool IsValid() const { return Value != 0; }
    };

    constexpr AssetID ASSET(const char* CanonicalPath) {
        return AssetID(Hash::FNV1A(CanonicalPath, Hash::detail::ConstexprStrLen(CanonicalPath)));
    }

    inline AssetID HashPath(const std::string& CanonicalPath) {
        return AssetID(Hash::FNV1A(CanonicalPath));
    }
}  // namespace Xen::PAK

namespace Xen {
    using PAK::ASSET;
    using PAK::AssetID;
}  // namespace Xen

template<>
struct std::hash<Xen::AssetID> {
    size_t operator()(const Xen::AssetID& ID) const noexcept { return std::hash<Xen::u64>()(ID.Value); }
};  // namespace std