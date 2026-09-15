//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Exception.hpp>

#include "Reflection.hpp"
#include "Scene.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <unordered_map>

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
        void Visit(const char* Name, glm::vec2& Value, const PropertyMeta&) override;
        void Visit(const char* Name, Transform& Value, const PropertyMeta&) override;
        void Visit(const char* Name, ActorHandle& Value, const PropertyMeta&) override;
        void Visit(const char* N, AssetID& V, const PropertyMeta& M) override;
        void Visit(const char* Name, Rect& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, glm::vec3& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, glm::vec4& Value, const PropertyMeta& Meta) override;

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
        void Visit(const char* Name, glm::vec2& Value, const PropertyMeta&) override;
        void Visit(const char* Name, Transform& Value, const PropertyMeta&) override;
        void Visit(const char* Name, ActorHandle& Value, const PropertyMeta&) override;
        void Visit(const char* N, AssetID& V, const PropertyMeta& Meta) override;
        void Visit(const char* Name, Rect& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, glm::vec3& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, glm::vec4& Value, const PropertyMeta& Meta) override;

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
        static constexpr u32 SCENE_FORMAT_VERSION = 1;

        static Json SaveToJson(const Scene& S);
        static void LoadFromJson(Scene& S, const Json& Root);

        static std::string SaveToString(const Scene& S, i32 Indent = 2);
        static void LoadFromString(Scene& S, const std::string& Text);

        static void SaveToFile(const Scene& S, const std::filesystem::path& Path, i32 Indent = 2);
        static void LoadFromFile(Scene& S, const std::filesystem::path& Path);

        static void LoadFromBytes(Scene& S, const u8* Bytes, size_t Size);
    };
}  // namespace Xen
