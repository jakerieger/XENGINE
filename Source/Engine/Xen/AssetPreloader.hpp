//
// Created by Jake Rieger on 9/9/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <XenPAK/AssetID.hpp>

#include "AssetLoader.hpp"
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

        /// Every recorded texture then mesh reference as a load request,
        /// deduplicated by asset - the first reference's Srgb wins if one
        /// image is referenced both ways.
        std::vector<LoadRequest> Requests() const;

    protected:
        void Visit(const char*, AssetID& Value, const PropertyMeta& Meta) override {
            if (Value.IsValid()) _Found.push_back({Value.Value, Meta.Asset, Meta.Srgb});
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
        struct Found {
            PAK::AssetIDValue Value {};
            AssetKind Kind {AssetKind::Unknown};
            bool Srgb {false};
        };
        std::vector<Found> _Found;
    };

    /// @brief Every asset a scene's actors and components reference.
    ///
    /// Deduplicated, so an atlas shared by forty sprites appears once.
    std::vector<AssetID> GatherSceneAssets(const Scene& S, AssetKind Kind);

    /// @brief Every texture and mesh a scene references, ready to hand to an
    /// AssetLoader (textures first, then meshes). Deduplicated, and carrying
    /// each texture's sRGB-ness from its reference (PropertyMeta::Srgb).
    std::vector<LoadRequest> GatherSceneLoadRequests(const Scene& S);

    /// @brief Loads every asset a scene references, synchronously on the
    /// calling thread. (Game loads a scene through AssetLoader instead, so a
    /// loading screen can keep running; this is the simple blocking version.)
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
