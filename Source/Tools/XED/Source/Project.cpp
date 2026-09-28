//
// Created by Jake Rieger on 9/28/2026.
//

#include "Project.hpp"

namespace Xen {
    std::optional<Project> ProjectSerializer::LoadFromFile(const std::filesystem::path& PrxjPath) {
        if (!exists(PrxjPath)) return None;

        const std::ifstream In(PrxjPath);
        if (!In) return None;

        std::ostringstream Buf;
        Buf << In.rdbuf();

        return LoadFromString(Buf.str(), PrxjPath.parent_path());
    }
    std::optional<Project> ProjectSerializer::LoadFromString(const std::string& JsonStr,
                                                             const std::filesystem::path& ProjectRoot) {
        Json Root = Json::parse(JsonStr);
        if (!Root.is_object()) return None;

        const auto VersionIt = Root.find("version");
        if (VersionIt == Root.end() || !VersionIt->is_number()) return None;
        const u32 Version = VersionIt->get<u32>();
        if (Version != XED_PROJECT_FORMAT_VERSION) return None;

        const auto EngineVersionIt = Root.find("engineVersion");
        if (EngineVersionIt == Root.end() || !EngineVersionIt->is_string()) return None;
        const std::string EngineVersion = EngineVersionIt->get<std::string>();
        if (EngineVersion.empty()) return None;

        const auto NameIt = Root.find("name");
        if (NameIt == Root.end() || !NameIt->is_string()) return None;
        const std::string Name = NameIt->get<std::string>();
        if (Name.empty()) return None;

        const auto DirectoriesIt = Root.find("directories");
        if (DirectoriesIt == Root.end() || !DirectoriesIt->is_object()) return None;

        const auto ConfigDirIt = DirectoriesIt->find("config");
        if (ConfigDirIt == DirectoriesIt->end() || !ConfigDirIt->is_string()) return None;
        const std::string ConfigDir = ConfigDirIt->get<std::string>();
        if (ConfigDir.empty()) return None;

        const auto ContentDirIt = DirectoriesIt->find("content");
        if (ContentDirIt == DirectoriesIt->end() || !ContentDirIt->is_string()) return None;
        const std::string ContentDir = ContentDirIt->get<std::string>();
        if (ContentDir.empty()) return None;

        const auto RuntimeDirIt = DirectoriesIt->find("runtime");
        if (RuntimeDirIt == DirectoriesIt->end() || !RuntimeDirIt->is_string()) return None;
        const std::string RuntimeDir = RuntimeDirIt->get<std::string>();
        if (RuntimeDir.empty()) return None;

        return Project {
          .Version       = Version,
          .EngineVersion = std::move(EngineVersion),
          .Name          = std::move(Name),
          .ProjectRoot   = ProjectRoot,
          // Project directories are always defined relative to the project root.
          .ConfigDirectory  = ProjectRoot / ConfigDir,
          .ContentDirectory = ProjectRoot / ContentDir,
          .RuntimeDirectory = ProjectRoot / RuntimeDir,
        };
    }
}  // namespace Xen