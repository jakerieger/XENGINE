//
// Created by Jake Rieger on 9/10/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include "Window.hpp"

#include <INIReader.h>

namespace Xen {
    struct EngineConfig {
        std::string StartupScene;
        Window::Mode WindowMode;
        u32 ResolutionX;
        u32 ResolutionY;

        static EngineConfig Read(const std::string& Path) {
            const INIReader Reader(Path);
            if (Reader.ParseError() < 0) {
                THROW_ENGINE_EXCEPTION(EngineException, "failed to load config file '" + Path + "'");
            }

            auto StrToMode = [](const std::string& Str) -> Window::Mode {
                if (Str == "windowed") { return Window::Mode::Windowed; }
                if (Str == "borderless") { return Window::Mode::Borderless; }
                if (Str == "fullscreen") { return Window::Mode::Fullscreen; }
                return Window::Mode::Fullscreen;
            };

            return {
              .StartupScene = Reader.Get("Engine", "StartupScene", ""),
              .WindowMode   = StrToMode(Reader.Get("Engine", "WindowMode", "fullscreen")),
              .ResolutionX  = Reader.GetUnsigned("Engine", "ResolutionX", 1280),
              .ResolutionY  = Reader.GetUnsigned("Engine", "ResolutionY", 720),
            };
        }
    };

    struct AudioConfig {
        f32 MasterVolume {1.0f};
        f32 MusicVolume {1.0f};
        f32 EffectsVolume {1.0f};
        f32 VoiceVolume {1.0f};
        f32 UIVolume {1.0f};

        static AudioConfig Read(const std::string& Path) {
            const INIReader Reader(Path);
            if (Reader.ParseError() < 0) {
                THROW_ENGINE_EXCEPTION(EngineException, "failed to load config file '" + Path + "'");
            }

            return {
              .MasterVolume  = CAST<f32>(Reader.GetReal("Audio", "MasterVolume", 1.0)),
              .MusicVolume   = CAST<f32>(Reader.GetReal("Audio", "MusicVolume", 1.0)),
              .EffectsVolume = CAST<f32>(Reader.GetReal("Audio", "EffectsVolume", 1.0)),
              .VoiceVolume   = CAST<f32>(Reader.GetReal("Audio", "VoiceVolume", 1.0)),
              .UIVolume      = CAST<f32>(Reader.GetReal("Audio", "UIVolume", 1.0)),
            };
        }
    };
}  // namespace Xen