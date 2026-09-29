//
// Created by Jake Rieger on 9/28/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Math.hpp>

namespace Xen {
    DEFINE_ENGINE_EXCEPTION(ColorException);

    class Color {
    public:
        Color() = default;
        Color(f32 R, f32 G, f32 B, f32 A = 1.0f);
        explicit Color(f32 V, f32 A = 1.0f);

        /// @brief Unsigned integer representation (ARGB).
        explicit Color(u32 V);

        /// @brief Web-style hex code string (#RRGGBB). Can either contain or emit the prefixed '#'.
        explicit Color(const std::string& Hex);

        /// @brief 8-bit component representation (0-255).
        explicit Color(u8 R, u8 G, u8 B, u8 A = 255);

        /// @brief DirectX XMFLOAT4 representation in sRGB (0-1) (RGBA).
        explicit Color(const Float4& V);

        /// @brief C-style constant float array of size 4 (RGBA).
        explicit Color(const f32 V[4]);

        Color(const Color& Other);
        Color& operator=(const Color& Other);
        Color(Color&& Other) noexcept;
        Color& operator=(Color&& Other) noexcept;
        bool operator==(const Color& Other) const;
        bool operator!=(const Color& Other) const;

        NODISCARD f32 R() const { return _R; }
        NODISCARD f32 G() const { return _G; }
        NODISCARD f32 B() const { return _B; }
        NODISCARD f32 A() const { return _A; }

        Color WithRed(f32 R) const;
        Color WithGreen(f32 G) const;
        Color WithBlue(f32 B) const;
        Color WithAlpha(f32 A) const;

        template<typename T>
        NODISCARD T To() const {
            return T {};
        }

    private:
        f32 _R {0.0f};
        f32 _G {0.0f};
        f32 _B {0.0f};
        f32 _A {1.0f};
    };

    /// @brief Converts to unsigned integer (ARGB).
    template<>
    NODISCARD inline u32 Color::To() const {
        const auto R = CAST<u8>(CAST<u32>(_R * 255.f));
        const auto G = CAST<u8>(CAST<u32>(_G * 255.f));
        const auto B = CAST<u8>(CAST<u32>(_B * 255.f));
        const auto A = CAST<u8>(CAST<u32>(_A * 255.f));
        return static_cast<u32>((A << 24) | (R << 16) | (G << 8) | B);
    }

    /// @brief Convert to web-style hex code with prefixed '#'. Ignores alpha component.
    template<>
    NODISCARD inline std::string Color::To() const {
        const auto R = CAST<u8>(CAST<u32>(_R * 255.f));
        const auto G = CAST<u8>(CAST<u32>(_G * 255.f));
        const auto B = CAST<u8>(CAST<u32>(_B * 255.f));

        std::ostringstream Stream;
        Stream << '#' << std::hex << std::setfill('0') << std::setw(2) << (R & 0xFF) << std::setw(2) << (G & 0xFF)
               << std::setw(2) << (B & 0xFF);

        return Stream.str();
    }

    template<>
    NODISCARD inline Float4 Color::To() const {
        return {_R, _G, _B, _A};
    }

    namespace Colors {
        static Color White {1.0f, 1.0f, 1.0f};
        static Color Black {0.0f, 0.0f, 0.0f};
        static Color Red {1.0f, 0.0f, 0.0f};
        static Color Green {0.0f, 1.0f, 0.0f};
        static Color Blue {0.0f, 0.0f, 1.0f};
        static Color Yellow {1.0f, 1.0f, 0.0f};
        static Color Magenta {1.0f, 0.0f, 1.0f};
        static Color Cyan {0.0f, 1.0f, 1.0f};
        static Color LightGrey {0.75f, 0.75f, 0.75f};
        static Color Grey {0.5f, 0.5f, 0.5f};
        static Color DarkGrey {0.25f, 0.25f, 0.25f};
        static Color White25 {1.0f, 1.0f, 1.0f, 0.25f};
        static Color White50 {1.0f, 1.0f, 1.0f, 0.5f};
        static Color White75 {1.0f, 1.0f, 1.0f, 0.75f};
        static Color Black25 {0.0f, 0.0f, 0.0f, 0.25f};
        static Color Black50 {0.0f, 0.0f, 0.0f, 0.5f};
        static Color Black75 {0.0f, 0.0f, 0.0f, 0.75f};
        static Color Transparent {0.0f, 0.0f};
    }  // namespace Colors
}  // namespace Xen

#ifndef X_COLOR_HASH_SPECIALIZATION
    #define X_COLOR_HASH_SPECIALIZATION
template<>
struct std::hash<Xen::Color> {
    std::size_t operator()(const Xen::Color& C) const noexcept { return std::hash<Xen::u32> {}(C.To<Xen::u32>()); }
};
#endif