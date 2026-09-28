//
// Created by Jake Rieger on 9/28/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <nlohmann/json.hpp>

namespace Xen {
    constexpr u32 XED_PROJECT_FORMAT_VERSION = 1;

    // Matches Xen::SceneSerializer's own Xen::Json alias exactly (both types,
    // not just the name) - nlohmann::json also aliases Xen::Json but to a
    // *different* underlying basic_json specialization, and two conflicting
    // aliases for the same name in the same namespace is a hard redefinition
    // error (C2371) the moment a single translation unit includes both
    // headers, as Editor.cpp now does.
    using Json = nlohmann::ordered_json;

    struct Project {
        u32 Version {1};
        std::string EngineVersion;
        std::string Name;
        std::filesystem::path ProjectRoot;  // This probably doesn't need to be stored, but I'm leaving it for now.
        std::filesystem::path ConfigDirectory;
        std::filesystem::path ContentDirectory;
        std::filesystem::path RuntimeDirectory;
    };

    class ProjectSerializer {
    public:
        static std::optional<Project> LoadFromFile(const std::filesystem::path& PrxjPath);
        static std::optional<Project> LoadFromString(const std::string& JsonStr,
                                                     const std::filesystem::path& ProjectRoot);

        // TODO: Save variants

    private:
        ProjectSerializer() = default;
    };
}  // namespace Xen
