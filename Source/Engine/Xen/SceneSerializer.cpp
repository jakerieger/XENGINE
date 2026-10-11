//
// Created by Jake Rieger on 9/8/2026.
//

#include <Common/Log.hpp>

#include "SceneSerializer.hpp"
#include "ComponentRegistry.hpp"
#include "PlaceholderComponent.hpp"

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
        T["Position"] = Json::array({V.Position.x, V.Position.y, V.Position.z});
        T["Rotation"] = Json::array({V.Rotation.x, V.Rotation.y, V.Rotation.z, V.Rotation.w});
        T["Scale"]    = Json::array({V.Scale.x, V.Scale.y, V.Scale.z});
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
        if (const auto It = J->find("Position"); It != J->end() && It->is_array() && It->size() >= 3) {
            V.Position.x = (*It)[0].get<f32>();
            V.Position.y = (*It)[1].get<f32>();
            V.Position.z = (*It)[2].get<f32>();
        }
        if (const auto It = J->find("Rotation"); It != J->end() && It->is_array() && It->size() >= 4) {
            V.Rotation.x = (*It)[0].get<f32>();
            V.Rotation.y = (*It)[1].get<f32>();
            V.Rotation.z = (*It)[2].get<f32>();
            V.Rotation.w = (*It)[3].get<f32>();
        }
        if (const auto It = J->find("Scale"); It != J->end() && It->is_array() && It->size() >= 3) {
            V.Scale.x = (*It)[0].get<f32>();
            V.Scale.y = (*It)[1].get<f32>();
            V.Scale.z = (*It)[2].get<f32>();
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

    namespace {
        // One actor's entry in a scene or prefab file. ParentID is the
        // persistent ID, within the same file, of its parent (0 for none).
        Json SaveActorEntry(const Scene& S, Actor& A, const u64 ParentID) {
            Json ActorObj      = Json::object();
            ActorObj["ID"]     = A.GetActorID();
            ActorObj["Parent"] = ParentID;

            JsonSaveReflector ActorReflector(S);
            A.Reflect(ActorReflector);
            ActorObj["Properties"] = std::move(ActorReflector.Result());

            Json Components = Json::array();
            for (size_t i = 0; i < A.GetComponentCount(); ++i) {
                IComponent* C = A.GetComponentAt(i);

                // A placeholder is a component whose type wasn't registered at
                // load: write back exactly what was read.
                if (const auto* Placeholder = dynamic_cast<const PlaceholderComponent*>(C)) {
                    Json CompObj          = Json::object();
                    CompObj["Type"]       = std::string(Placeholder->GetTypeName());
                    CompObj["Properties"] = Placeholder->GetProperties();
                    Components.push_back(std::move(CompObj));
                    continue;
                }

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

            return ActorObj;
        }
    }  // namespace

    Json SceneSerializer::SaveToJson(const Scene& S) {
        Json Root       = Json::object();
        Root["Version"] = SCENE_FORMAT_VERSION;
        Root["Name"]    = S.GetName();

        Json Actors = Json::array();
        S.ForEachActor([&](Actor& A) {
            // Parent is stored as the parent's persistent ID, not a handle.
            u64 ParentID = 0;
            if (const Actor* P = S.Get(A.GetParent())) ParentID = P->GetActorID();

            Actors.push_back(SaveActorEntry(S, A, ParentID));
        });

        Root["Actors"] = std::move(Actors);
        return Root;
    }

    SceneSerializer::SpawnedActors SceneSerializer::SpawnActors(Scene& S,
                                                                const Json& Actors,
                                                                const bool KeepIDs,
                                                                const SpawnPose* Pose,
                                                                const ActorHandle RootParent) {
        SpawnedActors Out;
        Out.All.reserve(Actors.size());

        // Saved ID -> the actor spawned for it. With fresh IDs these differ
        // from the saved ones, which is exactly why every reference in the
        // file goes through this map rather than the live scene.
        std::unordered_map<u64, ActorHandle> IDToHandle;

        try {
            // --- Pass 1: create every actor and record its saved ID.
            //
            // Must finish before any property loads: actor A can reference actor B
            // that appears later in the file, and two actors can reference each
            // other, so no single-pass ordering works.
            for (const Json& AJson : Actors) {
                if (!AJson.is_object()) {
                    THROW_ENGINE_EXCEPTION(SerializationException, "actor entry is not an object");
                }

                const auto IDIt   = AJson.find("ID");
                const u64 SavedID = IDIt != AJson.end() && IDIt->is_number() ? IDIt->get<u64>() : 0;

                const ActorHandle H = S.SpawnWithID(KeepIDs ? SavedID : 0);
                Out.All.push_back(H);
                if (SavedID != 0) IDToHandle[SavedID] = H;
            }

            // The parent each actor was saved with, if it's one of this batch.
            std::vector<ActorHandle> Parents;
            Parents.reserve(Actors.size());
            for (const Json& AJson : Actors) {
                ActorHandle Parent = ActorHandle::Invalid();
                if (const auto ParentIt = AJson.find("Parent"); ParentIt != AJson.end() && ParentIt->is_number()) {
                    const u64 ParentID = ParentIt->get<u64>();
                    if (const auto It = IDToHandle.find(ParentID); ParentID != 0 && It != IDToHandle.end()) {
                        Parent = It->second;
                    }
                }
                Parents.push_back(Parent);
            }
            for (size_t i = 0; i < Out.All.size(); ++i) {
                if (!Parents[i].IsSet()) Out.Roots.push_back(Out.All[i]);
            }

            // --- Pass 2: actors' own properties (name, transform, enabled).
            size_t Index = 0;
            for (const Json& AJson : Actors) {
                Actor* A = S.Get(Out.All[Index++]);
                if (!A) continue;

                if (const auto It = AJson.find("Properties"); It != AJson.end() && It->is_object()) {
                    JsonLoadReflector R(*It, IDToHandle);
                    A->Reflect(R);
                }
            }

            if (Pose && !Out.Roots.empty()) {
                if (Actor* Root = S.Get(Out.Roots.front())) {
                    Root->SetPosition(Pose->Position);
                    Root->SetRotation(Pose->Rotation);
                }
            }

            // --- Pass 3: parenting, once every actor exists.
            //
            // Before components load, not after: into a scene that has already
            // begun play, a component's BeginPlay runs the moment it's adopted
            // and may read the actor's world transform (a physics body is built
            // from it), which is only right once the hierarchy is in place.
            for (size_t i = 0; i < Out.All.size(); ++i) {
                Actor* A = S.Get(Out.All[i]);
                if (!A) continue;

                if (Parents[i].IsSet()) A->AttachTo(Parents[i]);
                else if (RootParent.IsSet()) A->AttachTo(RootParent);
            }

            // --- Pass 4: components, re-linking actor references.
            Index = 0;
            for (const Json& AJson : Actors) {
                Actor* A = S.Get(Out.All[Index++]);
                if (!A) continue;

                const auto CompsIt = AJson.find("Components");
                if (CompsIt == AJson.end() || !CompsIt->is_array()) continue;

                for (const Json& CJson : *CompsIt) {
                    const auto TypeIt = CJson.find("Type");
                    if (TypeIt == CJson.end() || !TypeIt->is_string()) continue;

                    const auto TypeName = TypeIt->get<std::string>();
                    auto Created        = ComponentRegistry::Get().Create(TypeName);
                    if (!Created) {
                        // Not registered - most likely it lives in a game module
                        // that isn't loaded. Keep the data in a placeholder so a
                        // later save doesn't drop it.
                        LOG_WARN("unknown component type '%s' on actor '%s' - kept as a placeholder",
                                 TypeName.c_str(),
                                 A->GetName().c_str());

                        const auto PropsIt = CJson.find("Properties");
                        A->AdoptComponent(std::make_unique<PlaceholderComponent>(
                          TypeName,
                          PropsIt != CJson.end() && PropsIt->is_object() ? *PropsIt : Json::object()));
                        continue;
                    }

                    if (const auto It = CJson.find("Properties"); It != CJson.end() && It->is_object()) {
                        JsonLoadReflector R(*It, IDToHandle);
                        Created->Reflect(R);
                    }

                    A->AdoptComponent(std::move(Created));
                }
            }
        } catch (const SerializationException&) {
            // All or nothing: a half-spawned batch (a bad entry, a parent cycle
            // in a hand-edited file) would leave stray actors behind.
            for (const ActorHandle H : Out.All)
                S.Destroy(H);
            throw;
        } catch (const std::exception& Error) {
            // What the file asked for was impossible (a parent cycle, say).
            // Reported the same way as any other malformed file.
            for (const ActorHandle H : Out.All)
                S.Destroy(H);
            THROW_ENGINE_EXCEPTION(SerializationException, std::string("invalid actor hierarchy: ") + Error.what());
        } catch (...) {
            for (const ActorHandle H : Out.All)
                S.Destroy(H);
            throw;
        }

        return Out;
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
        SpawnActors(S, *ActorsIt, true);

        if (const auto It = Root.find("Name"); It != Root.end() && It->is_string()) {
            S.SetName(It->get<std::string>());
        }
    }

    // --------------------------------------------------------------- prefabs

    Json SceneSerializer::SavePrefabToJson(const Scene& S, const ActorHandle RootHandle) {
        Actor* Root = S.Get(RootHandle);
        if (!Root) {
            THROW_ENGINE_EXCEPTION(SerializationException, "cannot save a prefab from an actor that doesn't exist");
        }

        // The root, then its descendants depth-first.
        std::vector<Actor*> Order;
        const auto Collect = [&](const auto& Self, Actor* A) -> void {
            Order.push_back(A);
            for (const ActorHandle Child : A->GetChildren()) {
                if (Actor* ChildActor = S.Get(Child)) Self(Self, ChildActor);
            }
        };
        Collect(Collect, Root);

        // A prefab has no parent, so the root is saved where it actually is in
        // the world rather than relative to a parent that isn't coming along.
        const Transform LocalTransform = Root->GetLocalTransform();
        Root->SetLocalTransform(Root->GetWorldTransform());

        Json Actors = Json::array();
        try {
            for (Actor* A : Order) {
                u64 ParentID = 0;
                if (A != Root) {
                    if (const Actor* P = S.Get(A->GetParent())) ParentID = P->GetActorID();
                }
                Actors.push_back(SaveActorEntry(S, *A, ParentID));
            }
        } catch (...) {
            Root->SetLocalTransform(LocalTransform);
            throw;
        }
        Root->SetLocalTransform(LocalTransform);

        Json Out       = Json::object();
        Out["Version"] = PREFAB_FORMAT_VERSION;
        Out["Type"]    = "Prefab";
        Out["Name"]    = Root->GetName();
        Out["Actors"]  = std::move(Actors);
        return Out;
    }

    void SceneSerializer::SavePrefabToFile(const Scene& S, const ActorHandle Root, const fs::path& Path, const i32 Indent) {
        // Built fully before the file is touched, so a failure can't truncate
        // an existing prefab.
        const std::string Text = SavePrefabToJson(S, Root).dump(Indent);

        // A new project has no prefabs folder yet.
        if (Path.has_parent_path()) {
            std::error_code Error;
            fs::create_directories(Path.parent_path(), Error);
        }

        std::ofstream Out(Path);
        if (!Out) {
            THROW_ENGINE_EXCEPTION(SerializationException, "could not open prefab file for writing: " + Path.string());
        }
        Out << Text;
    }

    ActorHandle SceneSerializer::InstantiatePrefab(Scene& S,
                                                   const Json& Prefab,
                                                   const SpawnPose* Pose,
                                                   const ActorHandle Parent) {
        if (!Prefab.is_object()) { THROW_ENGINE_EXCEPTION(SerializationException, "prefab root is not an object"); }

        const auto TypeIt = Prefab.find("Type");
        if (TypeIt == Prefab.end() || !TypeIt->is_string() || TypeIt->get<std::string>() != "Prefab") {
            THROW_ENGINE_EXCEPTION(SerializationException, "not a prefab (missing \"Type\": \"Prefab\")");
        }

        const auto VersionIt = Prefab.find("Version");
        if (VersionIt == Prefab.end() || !VersionIt->is_number()) {
            THROW_ENGINE_EXCEPTION(SerializationException, "prefab is missing a version");
        }
        if (VersionIt->get<u32>() != PREFAB_FORMAT_VERSION) {
            THROW_ENGINE_EXCEPTION(SerializationException,
                                   std::format("unsupported prefab version {} (expected {})",
                                               VersionIt->get<u32>(),
                                               PREFAB_FORMAT_VERSION));
        }

        const auto ActorsIt = Prefab.find("Actors");
        if (ActorsIt == Prefab.end() || !ActorsIt->is_array() || ActorsIt->empty()) {
            THROW_ENGINE_EXCEPTION(SerializationException, "prefab has no actors");
        }

        const SpawnedActors Spawned = SpawnActors(S, *ActorsIt, false, Pose, Parent);
        if (Spawned.Roots.empty()) {
            for (const ActorHandle H : Spawned.All)
                S.Destroy(H);
            THROW_ENGINE_EXCEPTION(SerializationException, "prefab has no root actor");
        }

        return Spawned.Roots.front();
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

    void SceneSerializer::SaveToFile(const Scene& S, const fs::path& Path, const i32 Indent) {
        std::ofstream Out(Path);
        if (!Out) {
            THROW_ENGINE_EXCEPTION(SerializationException, "could not open scene file for writing: " + Path.string());
        }
        Out << SaveToString(S, Indent);
    }

    void SceneSerializer::LoadFromFile(Scene& S, const fs::path& Path) {
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