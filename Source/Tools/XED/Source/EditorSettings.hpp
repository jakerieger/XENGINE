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

    enum class CodeEditor : u8 {
        None,
        VSCode,
        VSCodium,
        CLion,
    };

    inline std::optional<fs::path> GetCodeEditorPath(const CodeEditor Editor) {
        const auto LocalAppData = fs::path(std::getenv("LOCALAPPDATA"));
        const auto ProgramFiles = fs::path(std::getenv("PROGRAMFILES"));
        if (!exists(LocalAppData) || !exists(ProgramFiles)) return {};

        switch (Editor) {
            case CodeEditor::VSCode:
                return LocalAppData / "Programs" / "Microsoft VS Code" / "code.exe";
            case CodeEditor::VSCodium:
                return ProgramFiles / "VSCodium" / "VSCodium.exe";
            case CodeEditor::CLion:
                return LocalAppData / "Programs" / "CLion" / "bin" / "clion64.exe";
            default:
                return {};
        }
    }

    struct EditorSettings {
        fs::path StartupProject;
        EditorStartupMode StartupMode {EditorStartupMode::Maximized};
        std::string UITheme {"Dark"};

        struct {
            CodeEditor CodeEditor {CodeEditor::None};
        } ExternalTools;

        static EditorSettings Load(const fs::path& ConfigPath) {
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

            OutConfig.StartupProject  = Cfg["XED.Editor"]["StartupProject"];
            const auto StartupModeVal = Cfg["XED.Editor"]["StartupMode"];
            if (StartupModeVal == "maximized") OutConfig.StartupMode = EditorStartupMode::Maximized;
            else OutConfig.StartupMode = EditorStartupMode::Restore;
            OutConfig.UITheme = Cfg["XED.Editor"]["UITheme"];

            OutConfig.ExternalTools.CodeEditor = CodeEditor::None;
            const auto CodeEditorVal           = Cfg["XED.ExternalTools"]["CodeEditor"];
            if (CodeEditorVal == "vscode") {
                OutConfig.ExternalTools.CodeEditor = CodeEditor::VSCode;
            } else if (CodeEditorVal == "vscodium") {
                OutConfig.ExternalTools.CodeEditor = CodeEditor::VSCodium;
            } else if (CodeEditorVal == "clion") {
                OutConfig.ExternalTools.CodeEditor = CodeEditor::CLion;
            }

            return OutConfig;
        }

        NODISCARD bool Save() const {
            INI::Config Cfg;
            Cfg["XED.Editor"]["StartupProject"] = StartupProject.string();
            Cfg["XED.Editor"]["StartupMode"]    = StartupMode == EditorStartupMode::Maximized ? "maximized" : "restore";
            Cfg["XED.Editor"]["UITheme"]        = UITheme;

            Cfg["XED.ExternalTools"]["CodeEditor"] = "none";
            if (ExternalTools.CodeEditor == CodeEditor::VSCode) {
                Cfg["XED.ExternalTools"]["CodeEditor"] = "vscode";
            } else if (ExternalTools.CodeEditor == CodeEditor::VSCodium) {
                Cfg["XED.ExternalTools"]["CodeEditor"] = "vscodium";
            } else if (ExternalTools.CodeEditor == CodeEditor::CLion) {
                Cfg["XED.ExternalTools"]["CodeEditor"] = "clion";
            }

            try {
                INI::WriteToFile("Config/EditorSettings.ini", Cfg);
                return true;
            } catch (...) { return false; }
        }
    };
}  // namespace Xen