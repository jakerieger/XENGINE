//
// Created by Jake Rieger on 9/10/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include <ini.h>

namespace Xen {
    namespace Config {
        using IniFile = mINI::INIFile;
        using Ini     = mINI::INIStructure;

        inline Ini Read(const std::string& Path) {
            const IniFile File(Path);
            Ini Out;

            if (!File.read(Out)) {
                THROW_ENGINE_EXCEPTION(EngineException, "failed to load config file '" + Path + "'");
            }

            return Out;
        }

        inline u32 GetU32(const std::string& Val) {
            return CAST<u32>(std::stoi(Val));
        }

        inline f32 GetFloat(const std::string& Val) {
            return std::stof(Val);
        }

        inline std::vector<std::string> GetList(const std::string& Val) {
            auto Split = [](const std::string& value) -> std::vector<std::string> {
                std::vector<std::string> result;
                std::stringstream ss(value);
                std::string token;
                while (std::getline(ss, token, ',')) {
                    result.push_back(token);
                }
                return result;
            };

            return Split(Val);
        }
    }  // namespace Config

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

        static EngineConfig Read(const std::string& Path) {
            Config::Ini Cfg = Config::Read(Path);

            auto StrToMode = [](const std::string& Str) -> WindowMode {
                if (Str == "windowed") { return WindowMode::Windowed; }
                if (Str == "borderless") { return WindowMode::Borderless; }
                if (Str == "fullscreen") { return WindowMode::Fullscreen; }
                return WindowMode::Fullscreen;
            };

            return {
              .StartupScene = Cfg["Engine"]["StartupScene"],
              .Mode         = StrToMode(Cfg["Engine"]["WindowMode"]),
              .ResolutionX  = Config::GetU32(Cfg["Engine"]["ResolutionX"]),
              .ResolutionY  = Config::GetU32(Cfg["Engine"]["ResolutionY"]),
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
            Config::Ini Cfg = Config::Read(Path);

            AudioConfig Out;
            Out.MasterVolume  = Config::GetFloat(Cfg["Audio"]["MasterVolume"]);
            Out.MusicVolume   = Config::GetFloat(Cfg["Audio"]["MusicVolume"]);
            Out.EffectsVolume = Config::GetFloat(Cfg["Audio"]["EffectsVolume"]);
            Out.VoiceVolume   = Config::GetFloat(Cfg["Audio"]["VoiceVolume"]);
            Out.UIVolume      = Config::GetFloat(Cfg["Audio"]["UIVolume"]);

            return Out;
        }
    };
}  // namespace Xen