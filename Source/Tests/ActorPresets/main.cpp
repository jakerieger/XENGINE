//
// Created by Jake Rieger on 10/13/2026.
//
// Headless test for the editor's actor presets (XED's ActorPresets.cpp,
// compiled straight into this test): every preset spawns an actor named as
// asked with the components it promises, placed before its components begin
// play, in a scene that is already playing.
//
// Exit code 0 = all checks passed.

#include "ActorPresets.hpp"

#include <Xen/Actor.hpp>
#include <Xen/Components/CameraComponent.hpp>
#include <Xen/Components/CharacterControllerComponent.hpp>
#include <Xen/Components/DirectionalLightComponent.hpp>
#include <Xen/Components/EnvironmentComponent.hpp>
#include <Xen/Components/FPPlayerController.hpp>
#include <Xen/Components/PointLightComponent.hpp>
#include <Xen/Components/PostProcessComponent.hpp>
#include <Xen/Components/SpotLightComponent.hpp>
#include <Xen/EngineContext.hpp>
#include <Xen/Physics/PhysicsWorld.hpp>

#include <cmath>
#include <cstdio>
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

    void TestTable() {
        std::printf("preset table\n");

        std::set<std::string> Labels;
        std::set<int> Kinds;
        for (const ActorPresetInfo& Info : GetActorPresets()) {
            Labels.insert(Info.Label);
            Kinds.insert(CAST<int>(Info.Preset));
            CHECK(Info.Label && *Info.Label);
            CHECK(Info.DefaultName && *Info.DefaultName);
            CHECK(Info.Description && *Info.Description);
            CHECK(&GetActorPresetInfo(Info.Preset) == &Info);
        }
        CHECK(Labels.size() == GetActorPresets().size());  // no duplicates
        CHECK(Kinds.size() == GetActorPresets().size());
        CHECK(GetActorPresets().size() >= 8);
        CHECK(GetActorPresets()[0].Preset == ActorPreset::Empty);  // the one menus lead with
    }

    void TestSpawning() {
        std::printf("spawning each preset\n");

        PhysicsWorld World;
        EngineContext Context;
        Context.Physics = &World;
        Scene S("Presets", Context);
        S.BeginPlay();  // already playing: components begin as they're added

        const ActorHandle Empty = SpawnActorPreset(S, ActorPreset::Empty, "Nothing");
        Actor* A                = S.Get(Empty);
        CHECK(A && A->GetName() == "Nothing");
        CHECK(A && A->GetComponentCount() == 0);

        A = S.Get(SpawnActorPreset(S, ActorPreset::Camera, "Cam"));
        CHECK(A && A->GetName() == "Cam");
        CHECK(A && A->GetComponent<CameraComponent>() != nullptr);
        CHECK(A && A->GetComponent<CameraComponent>()->GetProjectionMode() == ProjectionMode::Perspective);
        CHECK(A && Near(A->GetComponent<CameraComponent>()->GetFieldOfView(), 60.0f));

        A = S.Get(SpawnActorPreset(S, ActorPreset::DirectionalLight, "Sun"));
        CHECK(A && A->GetComponent<DirectionalLightComponent>() != nullptr);
        CHECK(A && Near(A->GetComponent<DirectionalLightComponent>()->GetShadowDistance(), 15.0f));
        CHECK(A && !(Near(A->GetRotation().w, 1.0f, 1e-3f)));  // tilted, not left pointing straight ahead

        A = S.Get(SpawnActorPreset(S, ActorPreset::PointLight, "Bulb"));
        CHECK(A && A->GetComponent<PointLightComponent>() != nullptr);

        A = S.Get(SpawnActorPreset(S, ActorPreset::SpotLight, "Torch"));
        CHECK(A && A->GetComponent<SpotLightComponent>() != nullptr);
        CHECK(A && A->GetPosition().y > 0.0f);

        A = S.Get(SpawnActorPreset(S, ActorPreset::Environment, "Sky"));
        CHECK(A && A->GetComponent<EnvironmentComponent>() != nullptr);
        CHECK(A && A->GetComponent<EnvironmentComponent>()->GetMapAsset().IsValid());
        CHECK(A && A->GetComponent<EnvironmentComponent>()->GetShowBackground());

        A = S.Get(SpawnActorPreset(S, ActorPreset::PostProcess, "Fx"));
        CHECK(A && A->GetComponent<PostProcessComponent>() != nullptr);
        CHECK(A && A->GetComponent<PostProcessComponent>()->GetSettings().BloomEnabled);
        CHECK(A && A->GetComponent<PostProcessComponent>()->GetSettings().AutoExposureEnabled);

        // The player: a camera on a character, positioned before the capsule is built from it.
        A = S.Get(SpawnActorPreset(S, ActorPreset::Player, "Me"));
        CHECK(A && A->GetComponent<CameraComponent>() != nullptr);
        CHECK(A && A->GetComponent<FPPlayerController>() != nullptr);
        auto* Character = A ? A->GetComponent<CharacterControllerComponent>() : nullptr;
        CHECK(Character != nullptr);
        CHECK(Character && Character->HasCharacter());  // it was created at the actor's position, in the world
        CHECK(A && Near(A->GetPosition().y, 1.0f));

        // And it behaves: it falls to the ground and stands there.
        BodyDesc Ground;
        Ground.Motion            = BodyMotion::Static;
        Ground.Shape.Type        = ShapeType::Box;
        Ground.Shape.HalfExtents = {20.0f, 0.5f, 20.0f};
        Ground.Position          = {0.0f, -0.5f, 0.0f};
        CHECK(World.CreateBody(Ground) != InvalidBody);
        for (int i = 0; i < 120; ++i) {
            S.FixedTick(1.0f / 60.0f);
            World.Step(1.0f / 60.0f);
            S.SyncPhysics();
        }
        CHECK(Character && Character->IsGrounded());
        CHECK(A && Near(A->GetPosition().y, 0.9f, 0.06f));

        S.EndPlay();
    }

    void TestNames() {
        std::printf("names\n");

        Scene S("Names");
        for (const ActorPresetInfo& Info : GetActorPresets()) {
            Actor* A = S.Get(SpawnActorPreset(S, Info.Preset, Info.DefaultName));
            CHECK(A && A->GetName() == Info.DefaultName);
        }
    }
}  // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    TestTable();
    TestSpawning();
    TestNames();

    std::printf("\n%d checks, %d failed\n", Checks, Failures);
    return Failures == 0 ? 0 : 1;
}
