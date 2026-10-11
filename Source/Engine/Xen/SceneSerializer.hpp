//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Exception.hpp>

#include "Reflection.hpp"
#include "Scene.hpp"

#include <nlohmann/json.hpp>
#include <Common/Platform.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace Xen {
    using Json = nlohmann::ordered_json;

    DEFINE_ENGINE_EXCEPTION(SerializationException);

    class JsonSaveReflector final : public IReflector {
    public:
        explicit JsonSaveReflector(const Scene& S) : _Scene(&S) {}
        bool IsLoading() const override { return false; }
        Json& Result() { return _Out; }

    protected:
        void Visit(const char* Name, bool& Value, const PropertyMeta&) override;
        void Visit(const char* Name, i32& Value, const PropertyMeta&) override;
        void Visit(const char* Name, u32& Value, const PropertyMeta&) override;
        void Visit(const char* Name, u64& Value, const PropertyMeta&) override;
        void Visit(const char* Name, f32& Value, const PropertyMeta&) override;
        void Visit(const char* Name, f64& Value, const PropertyMeta&) override;
        void Visit(const char* Name, std::string& Value, const PropertyMeta&) override;
        void Visit(const char* Name, Float2& Value, const PropertyMeta&) override;
        void Visit(const char* Name, Transform& Value, const PropertyMeta&) override;
        void Visit(const char* Name, ActorHandle& Value, const PropertyMeta&) override;
        void Visit(const char* N, AssetID& V, const PropertyMeta& M) override;
        void Visit(const char* Name, Rect& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, Float3& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, Float4& Value, const PropertyMeta& Meta) override;

    private:
        const Scene* _Scene;
        Json _Out = Json::object();
    };

    class JsonLoadReflector final : public IReflector {
    public:
        JsonLoadReflector(const Json& In, const std::unordered_map<u64, ActorHandle>& IDToHandle)
            : _In(&In), _IDToHandle(&IDToHandle) {}

        bool IsLoading() const override { return true; }

    protected:
        void Visit(const char* Name, bool& Value, const PropertyMeta&) override;
        void Visit(const char* Name, i32& Value, const PropertyMeta&) override;
        void Visit(const char* Name, u32& Value, const PropertyMeta&) override;
        void Visit(const char* Name, u64& Value, const PropertyMeta&) override;
        void Visit(const char* Name, f32& Value, const PropertyMeta&) override;
        void Visit(const char* Name, f64& Value, const PropertyMeta&) override;
        void Visit(const char* Name, std::string& Value, const PropertyMeta&) override;
        void Visit(const char* Name, Float2& Value, const PropertyMeta&) override;
        void Visit(const char* Name, Transform& Value, const PropertyMeta&) override;
        void Visit(const char* Name, ActorHandle& Value, const PropertyMeta&) override;
        void Visit(const char* N, AssetID& V, const PropertyMeta& Meta) override;
        void Visit(const char* Name, Rect& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, Float3& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, Float4& Value, const PropertyMeta& Meta) override;

    private:
        template<typename Pred>
        const Json* Get(const char* Name, Pred IsExpectedKind) const {
            if (!_In || !_In->is_object()) return nullptr;
            const auto It = _In->find(Name);
            if (It == _In->end() || !IsExpectedKind(*It)) return nullptr;
            return &(*It);
        }

    private:
        const Json* _In;
        const std::unordered_map<u64, ActorHandle>* _IDToHandle;
    };

    class SceneSerializer {
    public:
        // Bumped for the Transform 2D->3D migration: Position/Scale gained a
        // Z component and Rotation changed from a scalar to a quaternion, so
        // a v1 file's Transform blocks don't parse the same way any more.
        static constexpr u32 SCENE_FORMAT_VERSION = 2;

        static Json SaveToJson(const Scene& S);
        static void LoadFromJson(Scene& S, const Json& Root);

        // --- Prefabs --------------------------------------------------------------------------------------------
        //
        // A prefab file is a scene file's "Actors" array for one actor and
        // its descendants, with the same per-actor entries - so it carries
        // every property, component and placeholder a scene would. The root
        // is the first entry; its Transform is saved in world space (a
        // prefab has no parent), and the file's IDs are only meaningful
        // inside it: instantiating assigns fresh IDs and re-links parent
        // and actor-reference properties between the copies. References to
        // actors outside the prefab don't survive (they load as unset).
        //
        // Instances are plain copies. Nothing links them back to the file.
        static constexpr u32 PREFAB_FORMAT_VERSION = 1;

        /// @brief Where to put a prefab's root when it's instantiated.
        /// Relative to the Parent, if one is given. Scale stays the prefab's.
        struct SpawnPose {
            Float3 Position {0.0f, 0.0f, 0.0f};
            Quat Rotation {IdentityQuat};
        };

        /// @brief Root and every descendant of Root, as a prefab document.
        /// Throws SerializationException if Root doesn't exist or one of its
        /// components isn't registered (it would be lost on load).
        static Json SavePrefabToJson(const Scene& S, ActorHandle Root);
        static void SavePrefabToFile(const Scene& S, ActorHandle Root, const fs::path& Path, i32 Indent = 2);

        /// @brief Spawns a copy of Prefab into S - under Parent if given,
        /// at Pose if given, else at the transform it was saved with.
        /// Returns the copy's root. The actors begin play immediately if the
        /// scene already has, with their parenting and transforms already in
        /// place. Throws SerializationException for a malformed prefab; on
        /// any failure nothing is left behind in the scene.
        static ActorHandle InstantiatePrefab(Scene& S,
                                             const Json& Prefab,
                                             const SpawnPose* Pose = nullptr,
                                             ActorHandle Parent    = ActorHandle::Invalid());

        struct SpawnedActors {
            std::vector<ActorHandle> All;
            /// Actors with no parent inside the batch, in file order.
            std::vector<ActorHandle> Roots;
        };

        /// @brief The spawn path shared by scenes and prefabs: creates an
        /// actor for each entry of an "Actors" array, loads their properties,
        /// parents them, then loads their components. KeepIDs keeps the
        /// saved IDs (loading a scene) rather than assigning fresh ones.
        static SpawnedActors SpawnActors(Scene& S,
                                         const Json& Actors,
                                         bool KeepIDs,
                                         const SpawnPose* Pose = nullptr,
                                         ActorHandle RootParent = ActorHandle::Invalid());

        static std::string SaveToString(const Scene& S, i32 Indent = 2);
        static void LoadFromString(Scene& S, const std::string& Text);

        static void SaveToFile(const Scene& S, const fs::path& Path, i32 Indent = 2);
        static void LoadFromFile(Scene& S, const fs::path& Path);

        static void LoadFromBytes(Scene& S, const u8* Bytes, size_t Size);
    };
}  // namespace Xen
