//
// Created by Jake Rieger on 9/10/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Ini.hpp>

namespace Xen {
    struct EngineConfig {
        enum class WindowMode : u8 {
            Windowed   = 0,
            Borderless = 1,
            Fullscreen = 2,
        };

        std::string StartupScene;
        WindowMode Mode {WindowMode::Borderless};
        u32 ResolutionX {1280};
        u32 ResolutionY {720};

        static EngineConfig Read(const std::filesystem::path& Path) {
            const auto ReadResult = INI::ReadFromFile(Path);
            if (!ReadResult.has_value()) { return {}; }

            INI::Config Cfg = *ReadResult;

            auto StrToMode = [](const std::string& Str) -> WindowMode {
                if (Str == "windowed") { return WindowMode::Windowed; }
                if (Str == "borderless") { return WindowMode::Borderless; }
                if (Str == "fullscreen") { return WindowMode::Fullscreen; }
                return WindowMode::Fullscreen;
            };

            return {
              .StartupScene = Cfg["Engine"]["StartupScene"],
              .Mode         = StrToMode(Cfg["Engine"]["WindowMode"]),
              .ResolutionX  = INI::GetU32(Cfg["Engine"]["ResolutionX"]),
              .ResolutionY  = INI::GetU32(Cfg["Engine"]["ResolutionY"]),
            };
        }
    };

    struct AudioConfig {
        f32 MasterVolume {1.0f};
        f32 MusicVolume {1.0f};
        f32 EffectsVolume {1.0f};
        f32 VoiceVolume {1.0f};
        f32 UIVolume {1.0f};

        static AudioConfig Read(const std::filesystem::path& Path) {
            const auto ReadResult = INI::ReadFromFile(Path);
            if (!ReadResult.has_value()) { return {}; }

            INI::Config Cfg = *ReadResult;

            AudioConfig Out;
            Out.MasterVolume  = INI::GetF32(Cfg["Audio"]["MasterVolume"]);
            Out.MusicVolume   = INI::GetF32(Cfg["Audio"]["MusicVolume"]);
            Out.EffectsVolume = INI::GetF32(Cfg["Audio"]["EffectsVolume"]);
            Out.VoiceVolume   = INI::GetF32(Cfg["Audio"]["VoiceVolume"]);
            Out.UIVolume      = INI::GetF32(Cfg["Audio"]["UIVolume"]);

            return Out;
        }
    };
}  // namespace Xen