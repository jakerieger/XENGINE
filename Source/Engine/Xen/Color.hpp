//
// Created by Jake Rieger on 9/28/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Math.hpp>

namespace Xen {
    DEFINE_ENGINE_EXCEPTION(ColorException);

    enum class ColorChannel : u8 { R = 0, G = 1, B = 2, A = 3 };

    /// @brief Ordered list of up to 4 color channels, built by chaining ColorChannels with '|'.
    class ColorChannelOrder {
    public:
        constexpr ColorChannelOrder(const ColorChannel First, const ColorChannel Second)
            : _Packed(static_cast<u8>(static_cast<u8>(First) | (static_cast<u8>(Second) << 2))), _Count(2) {}

        NODISCARD constexpr u32 Count() const { return _Count; }

        NODISCARD constexpr ColorChannel At(const u32 Index) const {
            return static_cast<ColorChannel>((_Packed >> (Index * 2)) & 0x3);
        }

        friend constexpr ColorChannelOrder operator|(ColorChannelOrder Order, const ColorChannel Next) {
            // A u32 holds at most 4 components; channels beyond the fourth are ignored.
            if (Order._Count >= 4) return Order;
            Order._Packed = static_cast<u8>(Order._Packed | (static_cast<u8>(Next) << (Order._Count * 2)));
            Order._Count++;
            return Order;
        }

    private:
        u8 _Packed;
        u8 _Count;
    };

    constexpr ColorChannelOrder operator|(const ColorChannel A, const ColorChannel B) {
        return {A, B};
    }

    /// @brief Bring into scope with `using namespace Xen::Channel;` to write `GetComponents(A | B | G | R)`.
    namespace Channel {
        inline constexpr auto R = ColorChannel::R;
        inline constexpr auto G = ColorChannel::G;
        inline constexpr auto B = ColorChannel::B;
        inline constexpr auto A = ColorChannel::A;
    }  // namespace Channel

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

        /// @brief Scales the RGB components by Brightness (0.0 = black, 1.0 = unchanged). Alpha is preserved.
        Color WithBrightness(f32 Brightness) const;

        /// @brief Blends the RGB components toward white by Amount (0.0 = unchanged, 1.0 = white). Alpha is preserved.
        Color Lightened(f32 Amount) const;

        /// @brief Packs the requested channels into a u32 as 8-bit values, in the order given. The first channel
        /// listed occupies the most significant byte of the result, e.g. `GetComponents(A | R | G | B)` is ARGB and
        /// `GetComponents(R | G | B)` is 0x00RRGGBB.
        NODISCARD u32 GetComponents(ColorChannel Channel) const;
        NODISCARD u32 GetComponents(ColorChannelOrder Order) const;

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