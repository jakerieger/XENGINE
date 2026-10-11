//
// Created by Jake Rieger on 9/14/2026.
//

#pragma once

#include <cstdint>
#include <string>

namespace Xen::Hash {
    namespace detail {
        constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
        constexpr uint64_t FNV_PRIME        = 1099511628211ULL;

        constexpr size_t ConstexprStrLen(const char* Str) {
            size_t Len = 0;
            while (Str[Len] != '\0')
                ++Len;
            return Len;
        }
    }  // namespace detail

    /// @brief Compile-time FNV1A hashing.
    constexpr uint64_t FNV1A(const char* Data, const size_t Len) {
        uint64_t Hash = detail::FNV_OFFSET_BASIS;

        for (size_t i = 0; i < Len; ++i) {
            Hash ^= static_cast<uint64_t>(static_cast<unsigned char>(Data[i]));
            Hash *= detail::FNV_PRIME;
        }

        return Hash;
    }

    /// @brief Run-time FNV1A hashing.
    inline uint64_t FNV1A(const std::string& Str) {
        return FNV1A(Str.data(), Str.size());
    }
}  // namespace Xen::Hash