//
// Created by Jake Rieger on 9/28/2026.
//

#include "EditorProject.hpp"

#include "Editor.hpp"

namespace Xen {
    std::optional<EditorProject> ProjectSerializer::LoadFromFile(const fs::path& PrxjPath) {
        if (!exists(PrxjPath)) return None;

        const std::ifstream In(PrxjPath);
        if (!In) return None;

        std::ostringstream Buf;
        Buf << In.rdbuf();

        return LoadFromString(Buf.str(), PrxjPath.parent_path());
    }

    std::optional<EditorProject> ProjectSerializer::LoadFromString(const std::string& JsonStr,
                                                                   const fs::path& ProjectRoot) {
        Json Root = Json::parse(JsonStr);
        if (!Root.is_object()) return None;

        const auto VersionIt = Root.find("version");
        if (VersionIt == Root.end() || !VersionIt->is_number()) return None;
        const u32 Version = VersionIt->get<u32>();
        if (Version != XED_PROJECT_FORMAT_VERSION) return None;

        const auto EngineVersionIt = Root.find("engineVersion");
        if (EngineVersionIt == Root.end() || !EngineVersionIt->is_string()) return None;
        const std::string EngineVersion = EngineVersionIt->get<std::string>();
        if (EngineVersion.empty() || EngineVersion != XEN_ENGINE_VERSION) return None;

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

        return EditorProject {
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

    void ProjectSerializer::SaveToFile(const EditorProject& Project, const fs::path& PrxjPath) {
        if (Project.Version != XED_PROJECT_FORMAT_VERSION) {
            THROW_ENGINE_EXCEPTION(EditorException, "Invalid format version");
        }
        if (Project.EngineVersion != XEN_ENGINE_VERSION) {
            THROW_ENGINE_EXCEPTION(EditorException, "Invalid engine version");
        }

        Json Root             = Json::object();
        Root["version"]       = Project.Version;
        Root["engineVersion"] = Project.EngineVersion;
        Root["name"]          = Project.Name;

        auto Directories       = Json::object();
        Directories["config"]  = Project.ConfigDirectory;
        Directories["content"] = Project.ContentDirectory;
        Directories["runtime"] = Project.RuntimeDirectory;

        Root["directories"] = std::move(Directories);

        std::ofstream Out(PrxjPath);
        if (!Out) { THROW_ENGINE_EXCEPTION(EditorException, "Failed to open .prxj file stream"); }
        Out << Root.dump(2);
        Out.close();

        if (!exists(PrxjPath)) { THROW_ENGINE_EXCEPTION(EditorException, "Failed to write .prxj file"); }
    }
}  // namespace Xen