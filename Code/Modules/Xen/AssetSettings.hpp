//
// Created by Jake Rieger on 9/14/2026.
//

#pragma once

#include <filesystem>
#include <vector>

namespace Xen {
    namespace PAK {
        struct AssetMountConfig;
    }

    struct AssetSettings {
        std::vector<std::filesystem::path> PakFiles;
        std::vector<std::filesystem::path> ContentDirs;
        bool MountContentDirs {false};  // Determined per build config
        bool AllowCommandLineContentDirs {true};
    };

    PAK::AssetMountConfig BuildMountConfig(const AssetSettings&, int argc, char* argv[]);
}  // namespace Xen