//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once
#pragma warning(disable : 4244)  // Type-conversion loss of data warning (i.e. float -> unsigned)

#include <cstdint>
#include <stdexcept>

namespace Xen {
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

#define _AssertBaseOf(Base, T) static_assert(std::is_base_of_v<Base, T>, "T must derive from " #Base ".");

#define _Kb(n) ((size_t)(n) * 1024)
#define _Mb(n) ((size_t)(n) * 1024 * 1024)
#define _Gb(n) ((size_t)(n) * 1024 * 1024 * 1024)

#define _NoDiscard [[nodiscard]]
}  // namespace Xen