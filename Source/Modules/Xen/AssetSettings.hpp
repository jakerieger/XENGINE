//
// Created by Jake Rieger on 9/14/2026.
//

#pragma once

#include <Common/Platform.hpp>
#include <vector>

namespace Xen {
    namespace PAK {
        struct AssetMountConfig;
    }

    struct AssetSettings {
        std::vector<fs::path> PakFiles;
        std::vector<fs::path> ContentDirs;
        bool MountContentDirs {false};  // Determined per build config
        bool AllowCommandLineContentDirs {true};

        // Debug builds only (see XenGameSettings.h.in) - empty in Release,
        // which is what keeps ShaderHotReload permanently inert there (see
        // ShaderHotReload.hpp). SourceDir is Source/Shaders (what to watch for
        // edits); OutputDir is EngineContent/Shaders (where a recompiled
        // shader's DXIL gets written - the same directory the offline build
        // already writes into and PAKTool already packs from).
        fs::path EngineShaderSourceDir;
        fs::path EngineShaderOutputDir;

        // Absolute path to dxc.exe, resolved at CMake configure time (see
        // XenGame.cmake) - the running game's own process PATH almost
        // certainly doesn't have the VS dev tools on it the way the build
        // itself does. Empty if dxc.exe couldn't be found at configure time
        // either, in which case ShaderHotReload falls back to a bare
        // "dxc.exe" PATH lookup (and will likely fail).
        fs::path EngineDxcPath;
    };

    PAK::AssetMountConfig BuildMountConfig(const AssetSettings&, int argc, char* argv[]);
}  // namespace Xen