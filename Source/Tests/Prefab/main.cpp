//
// Created by Jake Rieger on 10/9/2026.
//
// Headless test for prefabs: saving an actor hierarchy as a prefab document
// (SceneSerializer::SavePrefabToJson), instantiating it (SceneSerializer::
// InstantiatePrefab / Scene::Instantiate) with and without a pose and parent,
// loading it through the asset registry, rejecting bad files without leaving
// anything behind, and the PrefabSpawnerComponent. Needs no window or device.
//
// Exit code 0 = all checks passed.

#include <Xen/Actor.hpp>
#include <Xen/Components/PrefabSpawnerComponent.hpp>
#include <Xen/Components/RigidBodyComponent.hpp>
#include <Xen/EngineContext.hpp>
#include <Xen/Physics/PhysicsWorld.hpp>
#include <Xen/PlaceholderComponent.hpp>
#include <Xen/Scene.hpp>
#include <Xen/SceneSerializer.hpp>

#include <XenPAK/AssetRegistry.hpp>
#include <XenPAK/Canonicalize.hpp>
#include <XenPAK/LooseFileSource.hpp>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <set>
#include <string>

using namespace Xen;

namespace {
    int Failures = 0;
    int Checks   = 0;

    void Check(const bool Condition, const char* What, const int Line) {
        ++Checks;
        if (Condition) {
            std::printf("  ok    %s\n", What);
        } else {
            ++Failures;
            std::printf("  FAIL  %s (line %d)\n", What, Line);
        }
    }
#define CHECK(Cond) Check((Cond), #Cond, __LINE__)

    bool Near(const f32 A, const f32 B, const f32 Tolerance = 1e-3f) {
        return std::abs(A - B) <= Tolerance;
    }

    // A level: "Level" at x=100 holds "Crate" (local x=5, a dynamic body with a mass, plus a component type nothing
    // registers) which holds "Lid" (local y=1, another body). "Bystander" is unrelated.
    const char* SourceScene = R"({
  "Version": 2, "Name": "Source",
  "Actors": [
    { "ID": 10, "Parent": 0,
      "Properties": { "Name": "Level", "Enabled": true,
        "Transform": { "Position": [100, 0, 0], "Rotation": [0, 0, 0, 1], "Scale": [1, 1, 1] } },
      "Components": [] },
    { "ID": 20, "Parent": 10,
      "Properties": { "Name": "Crate", "Enabled": true,
        "Transform": { "Position": [5, 0, 0], "Rotation": [0, 0, 0, 1], "Scale": [1, 1, 1] } },
      "Components": [
        { "Type": "RigidBodyComponent", "Properties": { "Motion": 1, "Shape": 0, "Mass": 5.0, "Size": [1, 1, 1] } },
        { "Type": "SomeGameComponent", "Properties": { "Health": 42 } } ] },
    { "ID": 30, "Parent": 20,
      "Properties": { "Name": "Lid", "Enabled": true,
        "Transform": { "Position": [0, 1, 0], "Rotation": [0, 0, 0, 1], "Scale": [1, 1, 1] } },
      "Components": [
        { "Type": "RigidBodyComponent", "Properties": { "Motion": 1, "Shape": 0, "Size": [1, 1, 1] } } ] },
    { "ID": 40, "Parent": 0,
      "Properties": { "Name": "Bystander", "Enabled": true,
        "Transform": { "Position": [0, 0, 0], "Rotation": [0, 0, 0, 1], "Scale": [1, 1, 1] } },
      "Components": [] }
  ]
})";

    Actor* FindActor(Scene& S, const char* Name, const int Which = 0) {
        Actor* Found = nullptr;
        int Seen     = 0;
        S.ForEachActor([&](Actor& A) {
            if (A.GetName() == Name && Seen++ == Which) Found = &A;
        });
        return Found;
    }

    size_t ActorCount(Scene& S) {
        size_t Count = 0;
        S.ForEachActor([&](Actor&) { ++Count; });
        return Count;
    }

    // The saved prefab of "Crate" out of SourceScene.
    Json MakeCratePrefab() {
        Scene Source("Source");
        SceneSerializer::LoadFromString(Source, SourceScene);
        Actor* Crate = FindActor(Source, "Crate");
        return SceneSerializer::SavePrefabToJson(Source, Crate->GetHandle());
    }

    void TestSave() {
        std::printf("saving a prefab\n");

        Scene Source("Source");
        SceneSerializer::LoadFromString(Source, SourceScene);
        Actor* Crate = FindActor(Source, "Crate");
        const Transform LocalBefore = Crate->GetLocalTransform();

        const Json Prefab = SceneSerializer::SavePrefabToJson(Source, Crate->GetHandle());

        CHECK(Prefab["Version"] == SceneSerializer::PREFAB_FORMAT_VERSION);
        CHECK(Prefab["Type"] == "Prefab");
        CHECK(Prefab["Name"] == "Crate");
        CHECK(Prefab["Actors"].size() == 2);  // Crate and Lid, not Level or Bystander

        const Json& Root  = Prefab["Actors"][0];
        const Json& Child = Prefab["Actors"][1];
        CHECK(Root["Properties"]["Name"] == "Crate");
        CHECK(Root["Parent"] == 0);  // the Level above it isn't coming along
        CHECK(Child["Properties"]["Name"] == "Lid");
        CHECK(Child["Parent"] == Root["ID"]);

        // Saved where it is in the world (100 + 5), not relative to the Level.
        CHECK(Near(Root["Properties"]["Transform"]["Position"][0].get<f32>(), 105.0f));
        // ...without disturbing the actor it was read from.
        CHECK(Near(Crate->GetLocalTransform().Position.x, LocalBefore.Position.x, 1e-6f));
        CHECK(Near(Crate->GetWorldTransform().Position.x, 105.0f));

        CHECK(Root["Components"].size() == 2);
        CHECK(Root["Components"][0]["Properties"]["Mass"] == 5.0f);
        CHECK(Root["Components"][1]["Type"] == "SomeGameComponent");  // the unregistered one is kept

        SceneSerializer::SavePrefabToJson(Source, FindActor(Source, "Level")->GetHandle());  // a root with children works too

        bool Threw = false;
        try {
            SceneSerializer::SavePrefabToJson(Source, ActorHandle::Invalid());
        } catch (const SerializationException&) { Threw = true; }
        CHECK(Threw);
    }

    void TestInstantiate() {
        std::printf("instantiating a prefab\n");

        const Json Prefab = MakeCratePrefab();
        Scene Target("Target");

        // As saved, twice.
        const ActorHandle First  = SceneSerializer::InstantiatePrefab(Target, Prefab);
        const ActorHandle Second = SceneSerializer::InstantiatePrefab(Target, Prefab);
        CHECK(First.IsSet() && Second.IsSet() && First != Second);
        CHECK(ActorCount(Target) == 4);

        std::set<u64> IDs;
        Target.ForEachActor([&](Actor& A) { IDs.insert(A.GetActorID()); });
        CHECK(IDs.size() == 4);          // fresh, unique IDs - not the file's 20 and 30 twice
        CHECK(!IDs.contains(0));

        Actor* Crate = Target.Get(First);
        CHECK(Crate && Crate->GetName() == "Crate");
        CHECK(Crate->GetParent() == ActorHandle::Invalid());
        CHECK(Crate->GetChildren().size() == 1);
        Actor* Lid = Target.Get(Crate->GetChildren()[0]);
        CHECK(Lid && Lid->GetName() == "Lid");
        CHECK(Lid->GetParent() == First);                      // the second copy's lid isn't parented to the first crate
        CHECK(Near(Crate->GetWorldTransform().Position.x, 105.0f));
        CHECK(Near(Lid->GetWorldTransform().Position.y, 1.0f));

        // Components and their properties came across, including the placeholder.
        CHECK(Crate->GetComponentCount() == 2);
        CHECK(Crate->GetComponent<RigidBodyComponent>() != nullptr);
        CHECK(dynamic_cast<PlaceholderComponent*>(Crate->GetComponentAt(1)) != nullptr);
        const Json Resaved = SceneSerializer::SaveToJson(Target);
        bool FoundMass     = false;
        for (const Json& A : Resaved["Actors"]) {
            if (A["Properties"]["Name"] == "Crate" && A["Components"][0]["Properties"]["Mass"] == 5.0f) FoundMass = true;
        }
        CHECK(FoundMass);

        // The two copies are independent.
        Crate->SetPosition({0.0f, 0.0f, 0.0f});
        CHECK(Near(Target.Get(Second)->GetWorldTransform().Position.x, 105.0f));

        // At a pose.
        const SceneSerializer::SpawnPose Pose {{10.0f, 2.0f, 3.0f}, IdentityQuat};
        const ActorHandle Posed = SceneSerializer::InstantiatePrefab(Target, Prefab, &Pose);
        Actor* PosedCrate       = Target.Get(Posed);
        CHECK(Near(PosedCrate->GetWorldTransform().Position.x, 10.0f));
        CHECK(Near(PosedCrate->GetWorldTransform().Position.y, 2.0f));
        CHECK(Near(Target.Get(PosedCrate->GetChildren()[0])->GetWorldTransform().Position.y, 3.0f));  // the lid went along

        // Under a parent: the pose is relative to it.
        const ActorHandle AnchorHandle = Target.Spawn("Anchor");
        Target.Get(AnchorHandle)->SetPosition({50.0f, 0.0f, 0.0f});
        const SceneSerializer::SpawnPose Local {{1.0f, 0.0f, 0.0f}, IdentityQuat};
        const ActorHandle Child = SceneSerializer::InstantiatePrefab(Target, Prefab, &Local, AnchorHandle);
        CHECK(Target.Get(Child)->GetParent() == AnchorHandle);
        CHECK(Near(Target.Get(Child)->GetWorldTransform().Position.x, 51.0f));
    }

    void TestRejects() {
        std::printf("bad prefabs leave nothing behind\n");

        Scene Target("Target");
        Json Valid = MakeCratePrefab();

        const auto Rejected = [&](const Json& Bad) {
            const size_t Before = ActorCount(Target);
            bool Threw          = false;
            try {
                SceneSerializer::InstantiatePrefab(Target, Bad);
            } catch (const SerializationException&) { Threw = true; }
            return Threw && ActorCount(Target) == Before;
        };

        Json NotAPrefab = Valid;  // a scene document: wrong Type
        NotAPrefab.erase("Type");
        CHECK(Rejected(NotAPrefab));

        Json BadVersion       = Valid;
        BadVersion["Version"] = 99;
        CHECK(Rejected(BadVersion));

        Json NoActors = Valid;
        NoActors.erase("Actors");
        CHECK(Rejected(NoActors));

        Json EmptyActors      = Valid;
        EmptyActors["Actors"] = Json::array();
        CHECK(Rejected(EmptyActors));

        CHECK(Rejected(Json::array()));

        // Two actors parented to each other: spawning the first one succeeds, parenting the second fails.
        Json Cycle = Valid;
        Cycle["Actors"][0]["Parent"] = Cycle["Actors"][1]["ID"];
        CHECK(Rejected(Cycle));

        Json BadEntry = Valid;
        BadEntry["Actors"].push_back("not an actor");
        CHECK(Rejected(BadEntry));

        CHECK(ActorCount(Target) == 0);
    }

    // --- Through the asset registry --------------------------------------------------------------------------------

    void WriteFile(const fs::path& Path, const std::string& Text) {
        fs::create_directories(Path.parent_path());
        std::ofstream(Path) << Text;
    }

    AssetID PrefabID(const char* RelativePath) {
        return PAK::HashPath(PAK::Canonicalize(RelativePath));
    }

    void TestAssets() {
        std::printf("instantiating from the asset registry\n");

        const fs::path Dir = fs::temp_directory_path() / "xen_prefab_test";
        fs::remove_all(Dir);

        {
            Scene Source("Source");
            SceneSerializer::LoadFromString(Source, SourceScene);
            SceneSerializer::SavePrefabToFile(Source, FindActor(Source, "Crate")->GetHandle(), Dir / "prefabs" / "crate.xprefab");
        }
        WriteFile(Dir / "prefabs" / "broken.xprefab", "{ this is not json");
        WriteFile(Dir / "prefabs" / "scene.xprefab", SourceScene);  // a scene, not a prefab

        PAK::AssetRegistry Registry;
        Registry.AddSource(std::make_unique<PAK::LooseFileSource>(Dir, 100));

        EngineContext Context;
        Context.Assets = &Registry;
        Scene S("Target", Context);

        const ActorHandle Crate = S.Instantiate(PrefabID("prefabs/crate.xprefab"));
        CHECK(Crate.IsSet());
        CHECK(S.Get(Crate) && S.Get(Crate)->GetName() == "Crate");
        CHECK(ActorCount(S) == 2);

        const ActorHandle Posed = S.Instantiate(PrefabID("prefabs/crate.xprefab"), {1.0f, 2.0f, 3.0f});
        CHECK(Posed.IsSet());
        CHECK(Near(S.Get(Posed)->GetWorldTransform().Position.z, 3.0f));
        CHECK(ActorCount(S) == 4);

        const size_t Before = ActorCount(S);
        CHECK(!S.Instantiate(PrefabID("prefabs/missing.xprefab")).IsSet());  // not there
        CHECK(!S.Instantiate(AssetID {}).IsSet());                           // no asset set
        CHECK(!S.Instantiate(PrefabID("prefabs/broken.xprefab")).IsSet());   // not JSON
        CHECK(!S.Instantiate(PrefabID("prefabs/scene.xprefab")).IsSet());    // not a prefab
        CHECK(ActorCount(S) == Before);

        Scene NoAssets("NoAssets");
        CHECK(!NoAssets.Instantiate(PrefabID("prefabs/crate.xprefab")).IsSet());

        // The spawner: first copy on the first Tick, then one per 0.5 s, stopping at 3.
        Scene Spawning("Spawning", Context);
        const std::string SpawnerScene = std::string(R"({ "Version": 2, "Name": "S", "Actors": [
          { "ID": 1, "Parent": 0, "Properties": { "Name": "Spawner", "Enabled": true,
              "Transform": { "Position": [7, 8, 9], "Rotation": [0, 0, 0, 1], "Scale": [1, 1, 1] } },
            "Components": [ { "Type": "PrefabSpawnerComponent", "Properties": {
              "PrefabAsset": )") + std::to_string(PrefabID("prefabs/crate.xprefab").Value) + R"(, "Interval": 0.5, "MaxSpawns": 3 } } ] } ] })";
        SceneSerializer::LoadFromString(Spawning, SpawnerScene);
        Spawning.BeginPlay();

        Spawning.Tick(0.1f);
        CHECK(ActorCount(Spawning) == 3);  // spawner + crate + lid
        Actor* Spawned = FindActor(Spawning, "Crate");
        CHECK(Spawned && Near(Spawned->GetWorldTransform().Position.x, 7.0f) && Near(Spawned->GetWorldTransform().Position.y, 8.0f));
        for (int i = 0; i < 60; ++i)
            Spawning.Tick(0.1f);
        CHECK(ActorCount(Spawning) == 7);  // spawner + 3 * (crate + lid)

        fs::remove_all(Dir);
    }

    // Into a scene that's already playing, the copy's physics bodies must be built after the hierarchy is in place -
    // otherwise a child's body starts at its local offset instead of where it really is.
    void TestLiveScene() {
        std::printf("instantiating into a scene that is playing\n");

        PhysicsWorld World;
        EngineContext Context;
        Context.Physics = &World;

        Scene S("Live", Context);
        S.BeginPlay();

        const Json Prefab                 = MakeCratePrefab();
        const SceneSerializer::SpawnPose Pose {{10.0f, 0.0f, 0.0f}, IdentityQuat};
        const ActorHandle Root            = SceneSerializer::InstantiatePrefab(S, Prefab, &Pose);

        Actor* Crate = S.Get(Root);
        CHECK(Crate != nullptr);
        if (!Crate) return;
        Actor* Lid = S.Get(Crate->GetChildren()[0]);

        const auto* CrateBody = Crate->GetComponent<RigidBodyComponent>();
        const auto* LidBody   = Lid->GetComponent<RigidBodyComponent>();
        CHECK(CrateBody && CrateBody->HasBody());
        CHECK(LidBody && LidBody->HasBody());

        Float3 Position;
        Quat Rotation;
        World.GetBodyTransform(CrateBody->GetBody(), Position, Rotation);
        CHECK(Near(Position.x, 10.0f));
        World.GetBodyTransform(LidBody->GetBody(), Position, Rotation);
        CHECK(Near(Position.x, 10.0f));  // not 0 (its local offset)
        CHECK(Near(Position.y, 1.0f));

        S.EndPlay();
    }
}  // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);  // so a crash still shows how far it got
    TestSave();
    TestInstantiate();
    TestRejects();
    TestAssets();
    TestLiveScene();

    std::printf("\n%d checks, %d failed\n", Checks, Failures);
    return Failures == 0 ? 0 : 1;
}
