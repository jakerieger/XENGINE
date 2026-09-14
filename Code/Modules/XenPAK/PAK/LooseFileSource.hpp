//
// Created by Jake Rieger on 9/6/2026.
//

#pragma once

#include "PakCommon.hpp"
#include "AssetSource.hpp"

#include <filesystem>
#include <unordered_map>
#include <string>

namespace Xen::PAK {
    class LooseFileSource : public IAssetSource {
    public:
        LooseFileSource(std::filesystem::path RootDir, int Priority);

        bool Contains(AssetID ID) const override;
        AssetBuffer LoadFull(AssetID ID) override;
        int Priority() const override;
        const char* DebugName() const override;

        std::string DebugPathFor(AssetID ID) const;

        size_t AssetCount() const { return _PathMap.size(); }
        const std::filesystem::path& RootDir() const { return _RootDir; }

    private:
        void ScanDirectory();

        std::filesystem::path _RootDir;
        int _Priority;
        std::unordered_map<AssetIDValue, std::filesystem::path> _PathMap;
    };
}  // namespace Xen::PAK