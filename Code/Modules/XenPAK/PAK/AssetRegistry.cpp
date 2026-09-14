//
// Created by Jake Rieger on 9/6/2026.
//

#include "AssetRegistry.hpp"

#include <algorithm>
#include <stdexcept>

namespace Xen::PAK {
    void AssetRegistry::AddSource(std::unique_ptr<IAssetSource> Source) {
        _Sources.push_back(std::move(Source));
        SortByPriority();
    }

    bool AssetRegistry::Contains(const AssetID ID) const {
        return FindSource(ID) != nullptr;
    }

    AssetBuffer AssetRegistry::Load(const AssetID ID) const {
        for (auto& Source : _Sources) {
            if (Source->Contains(ID)) { return Source->LoadFull(ID); }
        }

        throw std::runtime_error(
          "AssetRegistry::Load - asset not found (id=" + std::to_string(ID.Value) + ")");
    }

    bool AssetRegistry::TryLoad(const AssetID ID, AssetBuffer& OutBuffer) const {
        for (auto& Source : _Sources) {
            if (Source->Contains(ID)) {
                OutBuffer = Source->LoadFull(ID);
                return true;
            }
        }

        return false;
    }

    const IAssetSource* AssetRegistry::FindSource(const AssetID ID) const {
        for (const auto& Source : _Sources) {
            if (Source->Contains(ID)) return Source.get();
        }

        return nullptr;
    }

    const IAssetSource* AssetRegistry::SourceAt(const size_t Index) const {
        return Index < _Sources.size() ? _Sources[Index].get() : nullptr;
    }

    void AssetRegistry::SortByPriority() {
        std::ranges::stable_sort(
          _Sources,
          [](const std::unique_ptr<IAssetSource>& a, const std::unique_ptr<IAssetSource>& b) {
              return a->Priority() > b->Priority();
          });
    }
}  // namespace Xen::PAK