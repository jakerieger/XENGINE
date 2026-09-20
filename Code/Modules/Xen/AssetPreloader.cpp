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

        for (const Found& F : _Found) {
            if (F.Kind != Kind) continue;
            if (Seen.insert(F.Value).second) Out.emplace_back(F.Value);
        }

        return Out;
    }

    std::vector<LoadRequest> AssetGatherer::Requests() const {
        std::unordered_set<PAK::AssetIDValue> Seen;
        std::vector<LoadRequest> Out;

        // Textures before meshes, matching the order PreloadSceneAssets always
        // used; each kind keeps its first-seen order.
        for (const AssetKind Kind : {AssetKind::Texture, AssetKind::Mesh}) {
            for (const Found& F : _Found) {
                if (F.Kind != Kind) continue;
                if (Seen.insert(F.Value).second) Out.push_back({AssetID {F.Value}, F.Kind, F.Srgb});
            }
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

    std::vector<LoadRequest> GatherSceneLoadRequests(const Scene& S) {
        AssetGatherer Gatherer;

        S.ForEachActor([&](Actor& A) {
            A.Reflect(Gatherer);
            for (size_t i = 0; i < A.GetComponentCount(); ++i) {
                if (IComponent* C = A.GetComponentAt(i)) C->Reflect(Gatherer);
            }
        });

        return Gatherer.Requests();
    }

    void PreloadSceneAssets(const Scene& S, const std::function<bool(size_t, size_t)>& Progress) {
        TextureCache* Textures = S.GetContext().Textures;
        MeshCache* Meshes      = S.GetContext().Meshes;

        // One kind at a time, each fed only its own kind's references -
        // feeding a mesh reference to the texture cache would decode raw
        // vertex bytes as an image, and vice versa.
        const std::vector<LoadRequest> Requests = GatherSceneLoadRequests(S);

        for (size_t i = 0; i < Requests.size(); ++i) {
            const LoadRequest& Request = Requests[i];
            if (Request.Kind == AssetKind::Texture && Textures) Textures->Preload(Request.ID, Request.Srgb);
            else if (Request.Kind == AssetKind::Mesh && Meshes) Meshes->Preload(Request.ID);

            if (Progress && !Progress(i + 1, Requests.size())) return;
        }
    }
}  // namespace Xen