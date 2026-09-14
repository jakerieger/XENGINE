//
// Created by Jake Rieger on 9/7/2026.
//

#pragma once

#include "AssetRegistry.hpp"
#include "PakFormat.hpp"
#include "PakCommon.hpp"

#include <filesystem>

namespace Xen::PAK {
    Pak_MakeException(MountException);

    /// @brief Describes everything the asset system should mount.
    ///
    /// Callers never assign priority numbers themselves - MountAssets derives
    /// them from list order, which removes the main way this gets
    /// misconfigured (two sources accidentally sharing a priority, or a pak
    /// silently outranking a dev's local edits).
    struct AssetMountConfig {
        std::vector<std::filesystem::path> PakFiles;
        std::vector<std::filesystem::path> ContentDirs;
    };

    /// @brief Priority bands. Paks occupy the low band, loose files sit above
    /// all of them. The step leaves room to insert sources between existing
    /// ones later without renumbering everything.
    constexpr int MOUNT_PRIORITY_PAK_BASE   = 0;
    constexpr int MOUNT_PRIORITY_LOOSE_BASE = 100000;
    constexpr int MOUNT_PRIORITY_STEP       = 10;

    /// @brief Builds a registry from a mount configuration.
    ///
    /// @throws MountException if a pak or loose directory is missing or if no
    ///         sources were configured at all.
    /// @throws InvalidPakException if a pak fails to open.
    /// @throws CryptoException if a pak fails to open.
    std::unique_ptr<AssetRegistry> MountAssets(const AssetMountConfig& Config);

    /// @brief Appends any --content-dir <path> arguments to Config.LooseDirs.
    ///
    /// Arguments are consumed in order, so multiple --content-dir flags mount
    /// multiple directories with the last one taking highest precedence.
    /// Unrecognized arguments are ignored, leaving the caller's own argument
    /// handling untouched.
    ///
    /// @throws MountException on a missing path argument, or if loose files
    ///         are disabled at compile time.
    void AppendContentDirsFromArgs(AssetMountConfig& Config, int argc, char* argv[]);

    /// @brief (dev) Human-readable summary of what a registry mounted.
    std::string DescribeMounts(const AssetRegistry& Registry);
}  // namespace Xen::PAK