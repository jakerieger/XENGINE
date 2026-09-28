//
// Created by Jake Rieger on 9/7/2026.
//

#pragma once

#include "AssetID.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace Xen::PAK {
    struct ScannedAsset {
        AssetID ID;
        std::filesystem::path AbsolutePath;
        std::string CanonicalPath;
    };

    enum class CollisionPolicy {
        /// @brief Used by LooseFileSource: avoids crashing over duplicate assets during
        /// development.
        WarnAndKeepFirst,

        /// @brief Used by the packing tool: should just fail the build on shipped builds.
        Throw,
    };

    std::vector<ScannedAsset> ScanContentDirectory(const std::filesystem::path& RootDir, CollisionPolicy Policy);
}  // namespace Xen::PAK