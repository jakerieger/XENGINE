//
// Created by Jake Rieger on 9/28/2026.
//

#pragma once

#include <Common/Ini.hpp>
#include <filesystem>

namespace Xen {
    enum class EditorStartupMode : u8 {
        Maximized,  // Always start with the editor window maximized
        Restore,    // Restore window to previous size/state
    };

    struct EditorConfig {
        std::filesystem::path CurrentProject;
        EditorStartupMode StartupMode {EditorStartupMode::Maximized};
        std::string UITheme {"dark"};

        static EditorConfig Read(const std::filesystem::path& ConfigPath) {
            if (!exists(ConfigPath)) {
                // TODO:
                LOG_WARN("EditorConfig.ini missing, creating a new one.");
                return {};
            }

            const auto ReadResult = INI::ReadFromFile(ConfigPath);
            if (!ReadResult.has_value()) {
                LOG_WARN("failed to read EditorConfig.ini, loading with defaults");
                return {};
            }

            INI::Config Cfg = *ReadResult;
            EditorConfig OutConfig;

            OutConfig.CurrentProject  = Cfg["Editor"]["CurrentProject"];
            const auto StartupModeVal = Cfg["Editor"]["StartupMode"];
            if (StartupModeVal == "maximized") OutConfig.StartupMode = EditorStartupMode::Maximized;
            if (StartupModeVal == "restore") OutConfig.StartupMode = EditorStartupMode::Restore;
            OutConfig.UITheme = Cfg["Editor"]["UITheme"];

            return OutConfig;
        }
    };
}  // namespace Xen