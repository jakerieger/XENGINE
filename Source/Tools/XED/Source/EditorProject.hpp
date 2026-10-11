//
// Created by Jake Rieger on 9/28/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Platform.hpp>
#include <nlohmann/json.hpp>

namespace Xen {
    constexpr u32 XED_PROJECT_FORMAT_VERSION = 1;

    using Json = nlohmann::ordered_json;

    struct EditorProject {
        u32 Version {1};
        std::string EngineVersion {XEN_ENGINE_VERSION};
        std::string Name;
        fs::path ProjectRoot;  // This probably doesn't need to be stored, but I'm leaving it for now.
        fs::path ConfigDirectory;
        fs::path ContentDirectory;
        fs::path RuntimeDirectory;

        /// Where XED looks for the project's built game module (<Name>Module.dll,
        /// searched recursively). Optional "moduleDirectory" in the .prxj, relative
        /// to the project root; defaults to <root>/build. Empty on a project that
        /// hasn't been loaded from a file yet, meaning "the default".
        fs::path ModuleDirectory;
    };

    class ProjectSerializer {
        ProjectSerializer() = delete;

    public:
        static std::optional<EditorProject> LoadFromFile(const fs::path& PrxjPath);
        static std::optional<EditorProject> LoadFromString(const std::string& JsonStr, const fs::path& ProjectRoot);

        static void SaveToFile(const EditorProject& Project, const fs::path& PrxjPath);
    };
}  // namespace Xen
