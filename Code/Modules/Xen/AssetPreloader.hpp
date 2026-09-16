//
// Created by Jake Rieger on 9/9/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <XenPAK/AssetID.hpp>

#include "Reflection.hpp"
#include "Scene.hpp"

#include <vector>

namespace Xen {
    /// @brief Reflector that records asset references instead of reading or
    /// writing values.
    class AssetGatherer final : public IReflector {
    public:
        bool IsLoading() const override { return false; }

        std::vector<AssetID> OfKind(AssetKind Kind) const;
        const std::vector<std::pair<PAK::AssetIDValue, AssetKind>>& All() const { return _Found; }

    protected:
        void Visit(const char*, AssetID& Value, const PropertyMeta& Meta) override {
            if (Value.IsValid()) _Found.emplace_back(Value.Value, Meta.Asset);
        }

        void Visit(const char* Name, bool& Value, const PropertyMeta& Meta) override {}
        void Visit(const char* Name, i32& Value, const PropertyMeta& Meta) override {}
        void Visit(const char* Name, u32& Value, const PropertyMeta& Meta) override {}
        void Visit(const char* Name, u64& Value, const PropertyMeta& Meta) override {}
        void Visit(const char* Name, f32& Value, const PropertyMeta& Meta) override {}
        void Visit(const char* Name, f64& Value, const PropertyMeta& Meta) override {}
        void Visit(const char* Name, std::string& Value, const PropertyMeta& Meta) override {}
        void Visit(const char* Name, Float2& Value, const PropertyMeta& Meta) override {}
        void Visit(const char* Name, Transform& Value, const PropertyMeta& Meta) override {}
        void Visit(const char* Name, Rect& Value, const PropertyMeta& Meta) override {}
        void Visit(const char* Name, ActorHandle& Value, const PropertyMeta& Meta) override {}
        void Visit(const char* Name, Float3& Value, const PropertyMeta& Meta) override {}
        void Visit(const char* Name, Float4& Value, const PropertyMeta& Meta) override {}

    private:
        std::vector<std::pair<PAK::AssetIDValue, AssetKind>> _Found;
    };

    /// @brief Every asset a scene's actors and components reference.
    ///
    /// Deduplicated, so an atlas shared by forty sprites appears once.
    std::vector<AssetID> GatherSceneAssets(const Scene& S, AssetKind Kind);

    /// @brief Loads every asset a scene references.
    ///
    /// Call after deserialization but BEFORE BeginPlay. Doing so means each
    /// component's Acquire is a cache hit rather than a synchronous load, so
    /// the disk and decode cost lands here - behind a loading screen - instead
    /// of hitching on the first frame.
    ///
    /// @param S Scene to load assets for
    /// @param Progress optional, called as (Done, Total) so a loading screen
    ///        can show a bar. Return false to abort.
    void PreloadSceneAssets(const Scene& S, const std::function<bool(size_t, size_t)>& Progress = {});
}  // namespace Xen
