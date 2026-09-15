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

        constexpr bool operator==(const AssetID& rhs) const { return Value == rhs.Value; }
        constexpr bool operator!=(const AssetID& rhs) const { return Value != rhs.Value; }
        constexpr bool operator<(const AssetID& rhs) const { return Value < rhs.Value; }
        constexpr bool operator>(const AssetID& rhs) const { return Value > rhs.Value; }
        constexpr bool operator<=(const AssetID& rhs) const { return Value <= rhs.Value; }
        constexpr bool operator>=(const AssetID& rhs) const { return Value >= rhs.Value; }

        [[nodiscard]] constexpr bool IsValid() const { return Value != 0; }
    };

    namespace Hash {
        constexpr u64 FNV_OFFSET_BASIS = 14695981039346656037ULL;
        constexpr u64 FNV_PRIME        = 1099511628211ULL;

        constexpr size_t CExprStrLen(const char* Str) {
            size_t Len = 0;
            while (Str[Len] != '\0')
                ++Len;
            return Len;
        }

        constexpr u64 FNV1A(const char* Data, const size_t Len) {
            u64 Hash = FNV_OFFSET_BASIS;
            for (size_t i = 0; i < Len; ++i) {
                Hash ^= CAST<u64>(CAST<unsigned char>(Data[i]));
                Hash *= FNV_PRIME;
            }
            return Hash;
        }

        inline u64 FNV1A(const std::string& Str) {
            return FNV1A(Str.data(), Str.size());
        }
    }  // namespace Hash

    constexpr AssetID ASSET(const char* CanonicalPath) {
        return AssetID(Hash::FNV1A(CanonicalPath, Hash::CExprStrLen(CanonicalPath)));
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
    size_t operator()(const Xen::AssetID& id) const noexcept { return std::hash<uint64_t>()(id.Value); }
};  // namespace std