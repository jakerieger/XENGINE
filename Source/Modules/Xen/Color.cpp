//
// Created by Jake Rieger on 9/28/2026.
//

#include "Color.hpp"

#include <algorithm>

namespace Xen {
    Color::Color(const f32 R, const f32 G, const f32 B, const f32 A) : _R(R), _G(G), _B(B), _A(A) {}
    Color::Color(const f32 V, const f32 A) : _R(V), _G(V), _B(V), _A(A) {}

    Color::Color(const u32 V) {
        _A = ((V >> 24) & 0xFF) / 255.f;
        _R = ((V >> 16) & 0xFF) / 255.f;
        _G = ((V >> 8) & 0xFF) / 255.f;
        _B = (V & 0xFF) / 255.f;
    }

    Color::Color(const std::string& Hex) {
        if (Hex.length() != 6 && Hex.length() != 7) {
            THROW_ENGINE_EXCEPTION(ColorException, "Invalid hex string (incorrect length).");
        }

        std::string H = Hex;
        if (H[0] == '#') H.erase(0, 1);

        const u32 R = std::strtoul(H.substr(0, 2).c_str(), nullptr, 16);
        const u32 G = std::strtoul(H.substr(2, 2).c_str(), nullptr, 16);
        const u32 B = std::strtoul(H.substr(4, 2).c_str(), nullptr, 16);

        _R = R / 255.f;
        _G = G / 255.f;
        _B = B / 255.f;
        _A = 1.f;
    }

    Color::Color(const u8 R, const u8 G, const u8 B, const u8 A) {
        _R = R / 255.f;
        _G = G / 255.f;
        _B = B / 255.f;
        _A = A / 255.f;
    }

    Color::Color(const Float4& V) {
        _R = V.x;
        _G = V.y;
        _B = V.z;
        _A = V.w;
    }

    Color::Color(const f32 V[4]) {
        _R = V[0];
        _G = V[1];
        _B = V[2];
        _A = V[3];
    }

    Color::Color(const Color& Other) : _R(Other._R), _G(Other._G), _B(Other._B), _A(Other._A) {}

    Color& Color::operator=(const Color& Other) {
        if (this != &Other) {
            _R = Other._R;
            _G = Other._G;
            _B = Other._B;
            _A = Other._A;
        }
        return *this;
    }

    Color::Color(Color&& Other) noexcept
        : _R(std::exchange(Other._R, 0.f)), _G(std::exchange(Other._G, 0.f)), _B(std::exchange(Other._B, 0.f)),
          _A(std::exchange(Other._A, 0.f)) {}

    Color& Color::operator=(Color&& Other) noexcept {
        if (this != &Other) {
            _R = std::exchange(Other._R, 0.f);
            _G = std::exchange(Other._G, 0.f);
            _B = std::exchange(Other._B, 0.f);
            _A = std::exchange(Other._A, 0.f);
        }
        return *this;
    }

    bool Color::operator==(const Color& Other) const {
        return _R == Other._R && _G == Other._G && _B == Other._B && _A == Other._A;
    }

    bool Color::operator!=(const Color& Other) const {
        return !(*this == Other);
    }

    u32 Color::GetComponents(const ColorChannel Channel) const {
        f32 Value = 0.f;
        switch (Channel) {
            case ColorChannel::R:
                Value = _R;
                break;
            case ColorChannel::G:
                Value = _G;
                break;
            case ColorChannel::B:
                Value = _B;
                break;
            case ColorChannel::A:
                Value = _A;
                break;
        }
        return CAST<u8>(CAST<u32>(Value * 255.f));
    }

    u32 Color::GetComponents(const ColorChannelOrder Order) const {
        u32 Result = 0;
        for (u32 I = 0; I < Order.Count(); ++I) {
            Result = (Result << 8) | GetComponents(Order.At(I));
        }
        return Result;
    }

    Color Color::WithRed(const f32 R) const {
        return {R, _G, _B, _A};
    }

    Color Color::WithGreen(f32 G) const {
        return {_R, G, _B, _A};
    }

    Color Color::WithBlue(f32 B) const {
        return {_R, _G, B, _A};
    }

    Color Color::WithAlpha(f32 A) const {
        return {_R, _G, _B, A};
    }

    Color Color::WithBrightness(f32 Brightness) const {
        Brightness = std::clamp(Brightness, 0.f, 1.f);
        return {_R * Brightness, _G * Brightness, _B * Brightness, _A};
    }

    Color Color::Lightened(f32 Amount) const {
        Amount = std::clamp(Amount, 0.f, 1.f);
        return {_R + (1.f - _R) * Amount, _G + (1.f - _G) * Amount, _B + (1.f - _B) * Amount, _A};
    }
}  // namespace Xen