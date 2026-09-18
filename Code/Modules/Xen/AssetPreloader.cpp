//
// Created by Jake Rieger on 9/9/2026.
//

#include "AssetPreloader.hpp"
#include "TextureCache.hpp"
#include "MeshCache.hpp"

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
        MeshCache* Meshes      = S.GetContext().Meshes;

        // One kind at a time, each fed only its own kind's references -
        // feeding a mesh reference to the texture cache would decode raw
        // vertex bytes as an image, and vice versa.
        std::vector<AssetID> Assets;
        if (Textures) {
            const std::vector<AssetID> TextureAssets = GatherSceneAssets(S, AssetKind::Texture);
            Assets.insert(Assets.end(), TextureAssets.begin(), TextureAssets.end());
        }
        const size_t TextureCount = Assets.size();

        if (Meshes) {
            const std::vector<AssetID> MeshAssets = GatherSceneAssets(S, AssetKind::Mesh);
            Assets.insert(Assets.end(), MeshAssets.begin(), MeshAssets.end());
        }

        for (size_t i = 0; i < Assets.size(); ++i) {
            if (i < TextureCount) Textures->Preload(Assets[i]);
            else Meshes->Preload(Assets[i]);

            if (Progress && !Progress(i + 1, Assets.size())) return;
        }
    }
}  // namespace Xen