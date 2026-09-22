//
// Created by Jake Rieger on 9/17/2026.
//

#pragma once

/// The Xen core library defines these at compile time. Other projects including this header and not linking against
/// Xen::Xen may not, so they're defined here just in case.
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN 1
#endif
#ifndef NOMINMAX
    #define NOMINMAX 1
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS 1
#endif

#include <Windows.h>

namespace Xen {
    constexpr unsigned long long operator""_KB(const unsigned long long N) {
        return N * 1024ULL;
    }

    constexpr unsigned long long operator""_MB(const unsigned long long N) {
        return N * 1024ULL * 1024ULL;
    }

    constexpr unsigned long long operator""_GB(const unsigned long long N) {
        return N * 1024ULL * 1024ULL * 1024ULL;
    }

#define KB(N) operator""_KB(N)
#define MB(N) operator""_MB(N)
#define GB(N) operator""_GB(N)

    constexpr f64 ToKB(const unsigned long long N) {
        return static_cast<f64>(N) / 1024.0;
    }

    constexpr f64 ToMB(const unsigned long long N) {
        return static_cast<f64>(N) / 1024.0 / 1024.0;
    }

    constexpr f64 ToGB(const unsigned long long N) {
        return static_cast<f64>(N) / 1024.0 / 1024.0 / 1024.0;
    }
}  // namespace Xen