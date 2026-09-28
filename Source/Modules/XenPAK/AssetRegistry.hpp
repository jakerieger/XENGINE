//
// Created by Jake Rieger on 9/6/2026.
//

#pragma once

#include "AssetSource.hpp"

#include <vector>
#include <memory>

namespace Xen::PAK {
    class AssetRegistry {
    public:
        void AddSource(std::unique_ptr<IAssetSource> Source);

        bool Contains(AssetID ID) const;
        AssetBuffer Load(AssetID ID) const;                      // Throws if not found
        bool TryLoad(AssetID ID, AssetBuffer& OutBuffer) const;  // Non-throw variant
        const IAssetSource* FindSource(AssetID ID) const;
        const IAssetSource* SourceAt(const size_t Index) const;

        size_t SourceCount() const { return _Sources.size(); }

    private:
        void SortByPriority();

        std::vector<std::unique_ptr<IAssetSource>> _Sources;
    };
}  // namespace Xen::PAK