//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once
#pragma warning(disable : 4244)  // Type-conversion loss of data warning (i.e. float -> unsigned)

#include <cstdint>

#include "Hash.hpp"
#include "Log.hpp"
#include "DateTime.hpp"
#include "Exception.hpp"

#include <cstdarg>

#pragma region Macros

#define ASSERT_BASE_OF(Base, T) static_assert(std::is_base_of_v<Base, T>, "T must derive from " #Base ".");
#define NODISCARD [[nodiscard]]
#define NORETURN [[noreturn]]

#define PANIC(Msg, ...) Xen::Panic(PSIG_HERE, __FILE__, __LINE__, #Msg, ##__VA_ARGS__);

#pragma endregion

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

    NORETURN inline void
    Panic(const std::string& Signature, const char* FileName, const u32 Line, const char* Fmt, ...) noexcept {
        va_list ArgsList;
        va_start(ArgsList, Fmt);
        char Msg[2048];
        vsnprintf(Msg, sizeof(Msg), Fmt, ArgsList);
        va_end(ArgsList);

        LOG_CRIT("(PANIC) %s:%d in %s: %s", FileName, Line, Signature.c_str(), Msg);

        std::abort();
    }

    inline constexpr std::_Ignore NoValue {};
}  // namespace Xen