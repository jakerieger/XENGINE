//
// Created by Jake Rieger on 9/7/2026.
//

#pragma once

#include <cstdint>
#include <format>

#define Pak_MakeExceptionStr(Msg) std::format("(EXCEPTION) XenPAK::{} - {}", __func__, Msg)

#define Pak_MakeException(Name)                                                                                        \
    class Name : public Xen::PAK::PakException {                                                                       \
    public:                                                                                                            \
        using Xen::PAK::PakException::PakException;                                                                    \
    }

namespace Xen::PAK {
    using u8   = uint8_t;
    using u16  = uint16_t;
    using u32  = uint32_t;
    using u64  = uint64_t;
    using uptr = std::uintptr_t;

    using i8   = int8_t;
    using i16  = int16_t;
    using i32  = int32_t;
    using i64  = int64_t;
    using iptr = intptr_t;

    using f32 = float;
    using f64 = double;

    template<typename T, typename U>
    constexpr T CAST(U Value) {
        return static_cast<T>(Value);
    }

    template<typename T, typename U>
    constexpr T CCAST(U Value) {
        return const_cast<T>(Value);
    }

    template<typename T, typename U>
    constexpr T DCAST(U Value) {
        return dynamic_cast<T>(Value);
    }

    template<typename T, typename U>
    constexpr T RCAST(U Value) {
        return reinterpret_cast<T>(Value);
    }

    class PakException : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };
}  // namespace Xen::PAK