//
// Created by Jake Rieger on 9/9/2026.
//

#include "AssetPreloader.hpp"
#include "TextureCache.hpp"

#include <unordered_set>

namespace Xen {
    std::vector<AssetID> AssetGatherer::OfKind(const AssetKind Kind) const {
        std::unordered_set<PAK::AssetIDValue> Seen;
        std::vector<AssetID> Out;

        for (const auto& [Value, K] : _Found) {
            if (K != Kind) continue;
            if (Seen.insert(Value).second) Out.emplace_back(Value);
        }

        return Out;
    }

    std::vector<AssetID> GatherSceneAssets(const Scene& S, const AssetKind Kind) {
        AssetGatherer Gatherer;

        S.ForEachActor([&](Actor& A) {
            A.Reflect(Gatherer);
            for (size_t i = 0; i < A.GetComponentCount(); ++i) {
                if (IComponent* C = A.GetComponentAt(i)) C->Reflect(Gatherer);
            }
        });

        return Gatherer.OfKind(Kind);
    }

    void PreloadSceneAssets(const Scene& S, const std::function<bool(size_t, size_t)>& Progress) {
        TextureCache* Textures = S.GetContext().Textures;
        if (!Textures) return;

        // Textures only. Feeding a scene or audio reference to the texture
        // cache would decode JSON as an image.
        const std::vector<AssetID> Assets = GatherSceneAssets(S, AssetKind::Texture);

        for (size_t i = 0; i < Assets.size(); ++i) {
            Textures->Preload(Assets[i]);

            if (Progress && !Progress(i + 1, Assets.size())) return;
        }
    }
}  // namespace Xen