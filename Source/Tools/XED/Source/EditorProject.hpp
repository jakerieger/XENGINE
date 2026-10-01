//
// Created by Jake Rieger on 9/28/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <nlohmann/json.hpp>

namespace Xen {
    constexpr u32 XED_PROJECT_FORMAT_VERSION = 1;

    using Json = nlohmann::ordered_json;

    struct EditorProject {
        u32 Version {1};
        std::string EngineVersion {XEN_ENGINE_VERSION};
        std::string Name;
        std::filesystem::path ProjectRoot;  // This probably doesn't need to be stored, but I'm leaving it for now.
        std::filesystem::path ConfigDirectory;
        std::filesystem::path ContentDirectory;
        std::filesystem::path RuntimeDirectory;
    };

    class ProjectSerializer {
        ProjectSerializer() = delete;

    public:
        static std::optional<EditorProject> LoadFromFile(const std::filesystem::path& PrxjPath);
        static std::optional<EditorProject> LoadFromString(const std::string& JsonStr,
                                                           const std::filesystem::path& ProjectRoot);

        static void SaveToFile(const EditorProject& Project, const std::filesystem::path& PrxjPath);
    };
}  // namespace Xen
