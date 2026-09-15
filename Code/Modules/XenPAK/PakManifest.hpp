//
// Created by Jake Rieger on 9/14/2026.
//

#pragma once

#include "AssetID.hpp"
#include "PakFormat.hpp"

#include <Xen/Exception.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace Xen::PAK {
    _DefineEngineException(InvalidManifestException);

    constexpr u32 PAK_MANIFEST_VERSION = 1;

    /// @brief Per-asset metadata that isn't recoverable from the .xpak itself - the pak only
    /// knows assets by their hashed AssetID, so recovering the original path/extension for
    /// tooling purposes requires this sidecar file.
    struct PakManifestEntry {
        AssetIDValue ID = 0;
        std::string CanonicalPath;
        u32 UncompressedSize = 0;
        u32 CompressedSize   = 0;
        PakCodec Codec       = PakCodec::None;
        bool Compressed      = false;
        bool Encrypted       = false;
    };

    /// @brief Dev-time-only sidecar to a .xpak file (same name, .xmeta extension) recording
    /// the metadata needed for tooling (`unpack`, `info`) to be useful. Never read by the
    /// runtime asset pipeline - only the packing tool produces/consumes it.
    struct PakManifest {
        u32 FormatVersion = PAK_MANIFEST_VERSION;
        std::string PakFilename;
        std::vector<PakManifestEntry> Assets;

        void WriteToFile(const std::filesystem::path& Path) const;

        /// @throws InvalidManifestException if the file is missing, malformed, or an
        /// unsupported manifest version.
        static PakManifest ReadFromFile(const std::filesystem::path& Path);

        /// @brief Sidecar manifest path for a given pak file: same directory + stem,
        /// ".xmeta" extension.
        static std::filesystem::path ManifestPathFor(const std::filesystem::path& PakPath);
    };
}  // namespace Xen::PAK
