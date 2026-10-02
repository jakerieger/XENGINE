//
// Created by Jake Rieger on 9/28/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Ini.hpp>
#include <Common/Platform.hpp>

namespace Xen {
    enum class EditorStartupMode : u8 {
        Maximized,  // Always start with the editor window maximized
        Restore,    // Restore window to previous size/state
    };

    struct EditorSettings {
        fs::path CurrentProject;
        EditorStartupMode StartupMode {EditorStartupMode::Maximized};
        std::string UITheme {"Dark.json"};

        struct {
            fs::path CMakePath;
            fs::path DxcPath;
        } ExternalTools;

        static EditorSettings Read(const fs::path& ConfigPath) {
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
            EditorSettings OutConfig;

            OutConfig.CurrentProject  = Cfg["XED.Editor"]["CurrentProject"];
            const auto StartupModeVal = Cfg["XED.Editor"]["StartupMode"];
            if (StartupModeVal == "maximized") OutConfig.StartupMode = EditorStartupMode::Maximized;
            if (StartupModeVal == "restore") OutConfig.StartupMode = EditorStartupMode::Restore;
            OutConfig.UITheme = Cfg["XED.Editor"]["UITheme"];

            OutConfig.ExternalTools.CMakePath = Cfg["XED.ExternalTools"]["CMakePath"];
            OutConfig.ExternalTools.DxcPath   = Cfg["XED.ExternalTools"]["DxcPath"];

            return OutConfig;
        }

        bool Write() const {
            INI::Config Cfg;
            Cfg["XED.Editor"]["CurrentProject"] = CurrentProject.string();
            Cfg["XED.Editor"]["StartupMode"]    = StartupMode == EditorStartupMode::Maximized ? "maximized" : "restore";
            Cfg["XED.Editor"]["UITheme"]        = UITheme;

            Cfg["XED.ExternalTools"]["CMakePath"] = ExternalTools.CMakePath.string();
            Cfg["XED.ExternalTools"]["DxcPath"]   = ExternalTools.DxcPath.string();

            try {
                INI::WriteToFile("Config/EditorSettings.ini", Cfg);
                return true;
            } catch (...) { return false; }
        }
    };
}  // namespace Xen