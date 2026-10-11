//
// Created by Jake Rieger on 10/6/2026.
//
// The editor's view of the project's Content directory: every file in it with
// its AssetID, display name and kind. The engine's own registries only map
// ID -> bytes, so anything that needs to show an asset to a human (the content
// browser, an inspector asset slot) goes through this instead.

#pragma once

#include <Common/Platform.hpp>
#include <Xen/Reflection.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Xen {
    /// @brief Classifies a file by extension. AssetKind::Unknown for anything
    /// the engine has no loader for.
    AssetKind AssetKindFromExtension(const fs::path& File);

    NODISCARD const char* AssetKindName(AssetKind Kind);

    /// @brief The ID the engine assigns a file at the given path relative to
    /// the content root (canonicalized, then hashed).
    AssetID AssetIDFromRelativePath(const fs::path& RelativePath);

    struct AssetEntry {
        AssetID ID;
        fs::path RelativePath;  // from the content root, native separators
        std::string Name;       // file name, with extension
        AssetKind Kind {AssetKind::Unknown};
    };

    class AssetIndex {
    public:
        /// @brief Rebuilds the index from Root's files. An empty or missing
        /// Root just leaves the index empty.
        void Rescan(const fs::path& Root);

        NODISCARD const fs::path& Root() const { return _Root; }

        /// @brief The entry for ID, or nullptr if no file in the content
        /// directory hashes to it (including the null ID).
        NODISCARD const AssetEntry* Find(AssetID ID) const;

    private:
        fs::path _Root;
        std::vector<AssetEntry> _Entries;
        std::unordered_map<AssetID, size_t> _ByID;
    };
}  // namespace Xen
