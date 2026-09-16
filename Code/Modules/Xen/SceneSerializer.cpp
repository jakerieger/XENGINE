//
// Created by Jake Rieger on 9/8/2026.
//

#include <Common/Log.hpp>

#include "SceneSerializer.hpp"
#include "ComponentRegistry.hpp"

#include <fstream>
#include <sstream>

namespace Xen {
    // ------------------------------------------------------------ save side

    void JsonSaveReflector::Visit(const char* N, bool& V, const PropertyMeta&) {
        _Out[N] = V;
    }
    void JsonSaveReflector::Visit(const char* N, i32& V, const PropertyMeta&) {
        _Out[N] = V;
    }
    void JsonSaveReflector::Visit(const char* N, u32& V, const PropertyMeta&) {
        _Out[N] = V;
    }
    void JsonSaveReflector::Visit(const char* N, u64& V, const PropertyMeta&) {
        _Out[N] = V;
    }
    void JsonSaveReflector::Visit(const char* N, f32& V, const PropertyMeta&) {
        _Out[N] = V;
    }
    void JsonSaveReflector::Visit(const char* N, f64& V, const PropertyMeta&) {
        _Out[N] = V;
    }

    void JsonSaveReflector::Visit(const char* N, std::string& V, const PropertyMeta&) {
        _Out[N] = V;
    }

    void JsonSaveReflector::Visit(const char* N, Float2& V, const PropertyMeta&) {
        _Out[N] = Json::array({V.x, V.y});
    }

    void JsonSaveReflector::Visit(const char* N, Transform& V, const PropertyMeta&) {
        Json T        = Json::object();
        T["Position"] = Json::array({V.Position.x, V.Position.y});
        T["Rotation"] = V.Rotation;
        T["Scale"]    = Json::array({V.Scale.x, V.Scale.y});
        _Out[N]       = std::move(T);
    }

    void JsonSaveReflector::Visit(const char* N, ActorHandle& V, const PropertyMeta&) {
        // Handles are runtime-only: index and generation depend on spawn order
        // and slot recycling, so they mean nothing in a file. Translate to the
        // target's persistent ID, or 0 when unset or already dangling.
        u64 ID = 0;
        if (_Scene) {
            if (const Actor* Target = _Scene->Get(V)) ID = Target->GetActorID();
        }
        _Out[N] = ID;
    }

    void JsonSaveReflector::Visit(const char* N, AssetID& V, const PropertyMeta&) {
        _Out[N] = V.Value;
    }

    void JsonSaveReflector::Visit(const char* N, Rect& V, const PropertyMeta&) {
        _Out[N] = Json::array({V.X, V.Y, V.Width, V.Height});
    }

    void JsonSaveReflector::Visit(const char* N, Float3& V, const PropertyMeta&) {
        _Out[N] = Json::array({V.x, V.y, V.z});
    }

    void JsonSaveReflector::Visit(const char* N, Float4& V, const PropertyMeta&) {
        _Out[N] = Json::array({V.x, V.y, V.z, V.w});
    }

    // ------------------------------------------------------------ load side

    void JsonLoadReflector::Visit(const char* N, bool& V, const PropertyMeta&) {
        if (const Json* J = Get(N, [](const Json& X) { return X.is_boolean(); })) { V = J->get<bool>(); }
    }
    void JsonLoadReflector::Visit(const char* N, i32& V, const PropertyMeta&) {
        if (const Json* J = Get(N, [](const Json& X) { return X.is_number(); })) { V = J->get<i32>(); }
    }
    void JsonLoadReflector::Visit(const char* N, u32& V, const PropertyMeta&) {
        if (const Json* J = Get(N, [](const Json& X) { return X.is_number(); })) { V = J->get<u32>(); }
    }
    void JsonLoadReflector::Visit(const char* N, u64& V, const PropertyMeta&) {
        if (const Json* J = Get(N, [](const Json& X) { return X.is_number(); })) { V = J->get<u64>(); }
    }
    void JsonLoadReflector::Visit(const char* N, f32& V, const PropertyMeta&) {
        if (const Json* J = Get(N, [](const Json& X) { return X.is_number(); })) { V = J->get<f32>(); }
    }
    void JsonLoadReflector::Visit(const char* N, f64& V, const PropertyMeta&) {
        if (const Json* J = Get(N, [](const Json& X) { return X.is_number(); })) { V = J->get<f64>(); }
    }

    void JsonLoadReflector::Visit(const char* N, std::string& V, const PropertyMeta&) {
        if (const Json* J = Get(N, [](const Json& X) { return X.is_string(); })) { V = J->get<std::string>(); }
    }

    void JsonLoadReflector::Visit(const char* N, Float2& V, const PropertyMeta&) {
        const Json* J = Get(N, [](const Json& X) { return X.is_array() && X.size() >= 2; });
        if (!J) return;
        V.x = (*J)[0].get<f32>();
        V.y = (*J)[1].get<f32>();
    }

    void JsonLoadReflector::Visit(const char* N, Transform& V, const PropertyMeta&) {
        const Json* J = Get(N, [](const Json& X) { return X.is_object(); });
        if (!J) return;

        // Each field is optional independently, so a transform saved before
        // Scale existed still loads its position and rotation.
        if (const auto It = J->find("Position"); It != J->end() && It->is_array() && It->size() >= 2) {
            V.Position.x = (*It)[0].get<f32>();
            V.Position.y = (*It)[1].get<f32>();
        }
        if (const auto It = J->find("Rotation"); It != J->end() && It->is_number()) { V.Rotation = It->get<f32>(); }
        if (const auto It = J->find("Scale"); It != J->end() && It->is_array() && It->size() >= 2) {
            V.Scale.x = (*It)[0].get<f32>();
            V.Scale.y = (*It)[1].get<f32>();
        }
    }

    void JsonLoadReflector::Visit(const char* N, ActorHandle& V, const PropertyMeta&) {
        const Json* J = Get(N, [](const Json& X) { return X.is_number(); });
        if (!J) return;

        const u64 ID = J->get<u64>();
        if (ID == 0) {
            V = ActorHandle::Invalid();
            return;
        }

        // An ID with no entry means the referenced actor wasn't in the file -
        // deleted since the save, or a partial file. Leave the reference unset
        // rather than pointing somewhere arbitrary.
        const auto It = _IDToHandle->find(ID);
        V             = It != _IDToHandle->end() ? It->second : ActorHandle::Invalid();
    }

    void JsonLoadReflector::Visit(const char* N, AssetID& V, const PropertyMeta&) {
        if (const Json* J = Get(N, [](const Json& X) { return X.is_number(); })) {
            V = AssetID {J->get<PAK::AssetIDValue>()};
        }
    }

    void JsonLoadReflector::Visit(const char* N, Rect& V, const PropertyMeta&) {
        const Json* J = Get(N, [](const Json& X) { return X.is_array() && X.size() >= 4; });
        if (!J) return;
        V.X      = (*J)[0].get<f32>();
        V.Y      = (*J)[1].get<f32>();
        V.Width  = (*J)[2].get<f32>();
        V.Height = (*J)[3].get<f32>();
    }

    void JsonLoadReflector::Visit(const char* N, Float3& V, const PropertyMeta&) {
        const Json* J = Get(N, [](const Json& X) { return X.is_array() && X.size() >= 3; });
        if (!J) return;
        V.x = (*J)[0].get<f32>();
        V.y = (*J)[1].get<f32>();
        V.z = (*J)[2].get<f32>();
    }

    void JsonLoadReflector::Visit(const char* N, Float4& V, const PropertyMeta&) {
        const Json* J = Get(N, [](const Json& X) { return X.is_array() && X.size() >= 4; });
        if (!J) return;
        V.x = (*J)[0].get<f32>();
        V.y = (*J)[1].get<f32>();
        V.z = (*J)[2].get<f32>();
        V.w = (*J)[3].get<f32>();
    }

    // ------------------------------------------------------- scene <-> json

    Json SceneSerializer::SaveToJson(const Scene& S) {
        Json Root       = Json::object();
        Root["Version"] = SCENE_FORMAT_VERSION;
        Root["Name"]    = S.GetName();

        Json Actors = Json::array();
        S.ForEachActor([&](Actor& A) {
            Json ActorObj  = Json::object();
            ActorObj["ID"] = A.GetActorID();

            // Parent is stored as the parent's persistent ID, not a handle.
            u64 ParentID = 0;
            if (const Actor* P = S.Get(A.GetParent())) ParentID = P->GetActorID();
            ActorObj["Parent"] = ParentID;

            JsonSaveReflector ActorReflector(S);
            A.Reflect(ActorReflector);
            ActorObj["Properties"] = std::move(ActorReflector.Result());

            Json Components = Json::array();
            for (size_t i = 0; i < A.GetComponentCount(); ++i) {
                IComponent* C = A.GetComponentAt(i);

                // An unregistered component can be written but never loaded,
                // so refuse now rather than emitting a file that silently
                // loses data later.
                if (!ComponentRegistry::Get().IsRegistered(C->GetTypeID())) {
                    THROW_ENGINE_EXCEPTION(
                      SerializationException,
                      std::format("component type '{}' on actor '{}' is not registered - it would be lost on "
                                  "load. Register it with ComponentRegistry::Register<T>().",
                                  C->GetTypeName(),
                                  A.GetName()));
                }

                Json CompObj    = Json::object();
                CompObj["Type"] = std::string(C->GetTypeName());

                JsonSaveReflector CompReflector(S);
                C->Reflect(CompReflector);
                CompObj["Properties"] = std::move(CompReflector.Result());

                Components.push_back(std::move(CompObj));
            }
            ActorObj["Components"] = std::move(Components);

            Actors.push_back(std::move(ActorObj));
        });

        Root["Actors"] = std::move(Actors);
        return Root;
    }

    void SceneSerializer::LoadFromJson(Scene& S, const Json& Root) {
        if (!Root.is_object()) { THROW_ENGINE_EXCEPTION(SerializationException, "scene root is not an object"); }

        const auto VersionIt = Root.find("Version");
        if (VersionIt == Root.end() || !VersionIt->is_number()) {
            THROW_ENGINE_EXCEPTION(SerializationException, "scene is missing a version");
        }
        if (VersionIt->get<u32>() != SCENE_FORMAT_VERSION) {
            THROW_ENGINE_EXCEPTION(
              SerializationException,
              std::format("unsupported scene version {} (expected {})", VersionIt->get<u32>(), SCENE_FORMAT_VERSION));
        }

        const auto ActorsIt = Root.find("Actors");
        if (ActorsIt == Root.end() || !ActorsIt->is_array()) {
            THROW_ENGINE_EXCEPTION(SerializationException, "scene has no Actors array");
        }

        S.Clear();

        // --- Pass 1: create every actor and record its saved ID.
        //
        // Must finish before any property loads: actor A can reference actor B
        // that appears later in the file, and two actors can reference each
        // other, so no single-pass ordering works.
        std::unordered_map<u64, ActorHandle> IDToHandle;
        std::vector<ActorHandle> Handles;
        Handles.reserve(ActorsIt->size());

        for (const Json& AJson : *ActorsIt) {
            if (!AJson.is_object()) { THROW_ENGINE_EXCEPTION(SerializationException, "actor entry is not an object"); }

            const auto IDIt   = AJson.find("ID");
            const u64 SavedID = IDIt != AJson.end() && IDIt->is_number() ? IDIt->get<u64>() : 0;

            const ActorHandle H = S.SpawnWithID(SavedID);
            Handles.push_back(H);
            if (SavedID != 0) IDToHandle[SavedID] = H;
        }

        // --- Pass 2: load properties and components, re-linking references.
        size_t Index = 0;
        for (const Json& AJson : *ActorsIt) {
            Actor* A = S.Get(Handles[Index++]);
            if (!A) continue;

            if (const auto It = AJson.find("Properties"); It != AJson.end() && It->is_object()) {
                JsonLoadReflector R(*It, IDToHandle);
                A->Reflect(R);
            }

            const auto CompsIt = AJson.find("Components");
            if (CompsIt == AJson.end() || !CompsIt->is_array()) continue;

            for (const Json& CJson : *CompsIt) {
                const auto TypeIt = CJson.find("Type");
                if (TypeIt == CJson.end() || !TypeIt->is_string()) continue;

                const auto TypeName = TypeIt->get<std::string>();
                auto Created        = ComponentRegistry::Get().Create(TypeName);
                if (!Created) {
                    THROW_ENGINE_EXCEPTION(SerializationException,
                                          std::format("unknown component type '{}' on actor '{}' - was it registered?",
                                                      TypeName,
                                                      A->GetName()));
                }

                if (const auto It = CJson.find("Properties"); It != CJson.end() && It->is_object()) {
                    JsonLoadReflector R(*It, IDToHandle);
                    Created->Reflect(R);
                }

                A->AdoptComponent(std::move(Created));
            }
        }

        // --- Pass 3: re-establish parenting, once every actor exists.
        Index = 0;
        for (const Json& AJson : *ActorsIt) {
            Actor* A = S.Get(Handles[Index++]);
            if (!A) continue;

            const auto ParentIt = AJson.find("Parent");
            if (ParentIt == AJson.end() || !ParentIt->is_number()) continue;

            const u64 ParentID = ParentIt->get<u64>();
            if (ParentID == 0) continue;

            if (const auto It = IDToHandle.find(ParentID); It != IDToHandle.end()) { A->AttachTo(It->second); }
        }

        if (const auto It = Root.find("Name"); It != Root.end() && It->is_string()) {
            S.SetName(It->get<std::string>());
        }
    }

    std::string SceneSerializer::SaveToString(const Scene& S, const i32 Indent) {
        return SaveToJson(S).dump(Indent);
    }

    void SceneSerializer::LoadFromString(Scene& S, const std::string& Text) {
        try {
            LoadFromJson(S, Json::parse(Text));
        } catch (const Json::parse_error& Ex) {
            // Translated so callers only ever have to catch our exception type
            // rather than nlohmann's as well.
            THROW_ENGINE_EXCEPTION(SerializationException, std::string("malformed scene JSON: ") + Ex.what());
        }
    }

    void SceneSerializer::SaveToFile(const Scene& S, const std::filesystem::path& Path, const i32 Indent) {
        std::ofstream Out(Path);
        if (!Out) {
            THROW_ENGINE_EXCEPTION(SerializationException, "could not open scene file for writing: " + Path.string());
        }
        Out << SaveToString(S, Indent);
    }

    void SceneSerializer::LoadFromFile(Scene& S, const std::filesystem::path& Path) {
        std::ifstream In(Path);
        if (!In) { THROW_ENGINE_EXCEPTION(SerializationException, "could not open scene file: " + Path.string()); }
        std::ostringstream Buf;
        Buf << In.rdbuf();
        LoadFromString(S, Buf.str());
    }

    void SceneSerializer::LoadFromBytes(Scene& S, const u8* Bytes, const size_t Size) {
        LoadFromString(S, std::string(RCAST<const char*>(Bytes), Size));
    }
}  // namespace Xen