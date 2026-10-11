//
// Created by Jake Rieger on 10/6/2026.
//
// Headless test for the physics layer (Xen/Physics/PhysicsWorld.hpp) and the
// RigidBody / CharacterController components driving it. Needs no window or
// render device: the world is stepped by hand, exactly like Game::TickFrame
// does (Scene::FixedTick -> PhysicsWorld::Step -> Scene::SyncPhysics).
//
// Exit code 0 = all checks passed.

#include <Xen/Actor.hpp>
#include <Xen/Components/CharacterControllerComponent.hpp>
#include <Xen/EngineContext.hpp>
#include <Xen/Physics/PhysicsWorld.hpp>
#include <Xen/Scene.hpp>
#include <Xen/SceneSerializer.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
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

    constexpr f32 Dt = 1.0f / 60.0f;

    bool Near(const f32 A, const f32 B, const f32 Tolerance) {
        return std::abs(A - B) <= Tolerance;
    }

    // A big static slab whose top surface is at y = 0.
    BodyHandle AddFloor(PhysicsWorld& World) {
        BodyDesc Desc;
        Desc.Motion               = BodyMotion::Static;
        Desc.Shape.Type           = ShapeType::Box;
        Desc.Shape.HalfExtents    = {50.0f, 0.5f, 50.0f};
        Desc.Position             = {0.0f, -0.5f, 0.0f};
        return World.CreateBody(Desc);
    }

    BodyHandle AddBox(PhysicsWorld& World, const Float3& Position, const BodyMotion Motion = BodyMotion::Dynamic) {
        BodyDesc Desc;
        Desc.Motion            = Motion;
        Desc.Shape.Type        = ShapeType::Box;
        Desc.Shape.HalfExtents = {0.5f, 0.5f, 0.5f};
        Desc.Position          = Position;
        return World.CreateBody(Desc);
    }

    void Step(PhysicsWorld& World, const int Count) {
        for (int i = 0; i < Count; ++i)
            World.Step(Dt);
    }

    Float3 PositionOf(const PhysicsWorld& World, const BodyHandle Body) {
        Float3 Position;
        Quat Rotation;
        World.GetBodyTransform(Body, Position, Rotation);
        return Position;
    }

    void TestFall() {
        std::printf("falling body\n");

        PhysicsWorld World;
        const BodyHandle Box = AddBox(World, {0.0f, 5.0f, 0.0f});
        CHECK(Box != InvalidBody);
        CHECK(World.IsValid(Box));

        // Half a second of free fall: ~1.2 m (a little less with damping).
        Step(World, 30);
        const f32 Y = PositionOf(World, Box).y;
        CHECK(Y < 4.0f && Y > 3.5f);
        CHECK(World.GetLinearVelocity(Box).y < -3.0f);
    }

    void TestRestsOnFloor() {
        std::printf("box comes to rest on a floor\n");

        PhysicsWorld World;
        AddFloor(World);
        const BodyHandle Box = AddBox(World, {0.0f, 5.0f, 0.0f});

        Step(World, 240);
        const Float3 Position = PositionOf(World, Box);
        CHECK(Near(Position.y, 0.5f, 0.05f));
        CHECK(Near(Position.x, 0.0f, 0.05f));
        CHECK(std::abs(World.GetLinearVelocity(Box).y) < 0.05f);
    }

    void TestMeshFloor() {
        std::printf("box rests on a triangle-mesh floor\n");

        PhysicsWorld World;

        BodyDesc Floor;
        Floor.Motion         = BodyMotion::Static;
        Floor.Shape.Type     = ShapeType::Mesh;
        Floor.Shape.Vertices = {{-20.0f, 0.0f, -20.0f}, {20.0f, 0.0f, -20.0f}, {20.0f, 0.0f, 20.0f}, {-20.0f, 0.0f, 20.0f}};
        Floor.Shape.Indices  = {0, 2, 1, 0, 3, 2};
        CHECK(World.CreateBody(Floor) != InvalidBody);

        const BodyHandle Box = AddBox(World, {0.0f, 3.0f, 0.0f});
        Step(World, 240);
        CHECK(Near(PositionOf(World, Box).y, 0.5f, 0.06f));

        // A mesh shape can't be simulated.
        BodyDesc BadDesc    = Floor;
        BadDesc.Motion      = BodyMotion::Dynamic;
        CHECK(World.CreateBody(BadDesc) == InvalidBody);
    }

    void TestRaycast() {
        std::printf("raycast\n");

        PhysicsWorld World;
        const BodyHandle Floor = AddFloor(World);
        const BodyHandle Box   = AddBox(World, {0.0f, 3.0f, 0.0f}, BodyMotion::Static);
        (void)Box;

        const RaycastHit Down = World.Raycast({10.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 20.0f);
        CHECK(Down.Hit);
        CHECK(Near(Down.Distance, 5.0f, 0.01f));
        CHECK(Near(Down.Point.y, 0.0f, 0.01f));
        CHECK(Near(Down.Normal.y, 1.0f, 0.01f));
        CHECK(Down.Body == Floor);

        CHECK(!World.Raycast({10.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 2.0f).Hit);  // too short
        CHECK(!World.Raycast({10.0f, 5.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 50.0f).Hit);  // away from everything

        // Ignoring the floor leaves nothing below.
        CHECK(!World.Raycast({10.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 20.0f, Floor).Hit);
    }

    void TestTeleportAndKinematic() {
        std::printf("teleport and kinematic bodies\n");

        PhysicsWorld World;
        const BodyHandle Box = AddBox(World, {0.0f, 20.0f, 0.0f});
        Step(World, 30);
        CHECK(World.GetLinearVelocity(Box).y < -3.0f);

        World.SetBodyTransform(Box, {7.0f, 40.0f, 0.0f}, IdentityQuat);
        CHECK(Near(PositionOf(World, Box).x, 7.0f, 0.001f));
        CHECK(Near(World.GetLinearVelocity(Box).y, 0.0f, 0.001f));

        const BodyHandle Platform = AddBox(World, {100.0f, 0.0f, 0.0f}, BodyMotion::Kinematic);
        for (int i = 0; i < 60; ++i) {
            const f32 X = 100.0f + CAST<f32>(i + 1) * 0.1f;
            World.MoveKinematic(Platform, {X, 0.0f, 0.0f}, IdentityQuat, Dt);
            World.Step(Dt);
        }
        CHECK(Near(PositionOf(World, Platform).x, 106.0f, 0.05f));
        CHECK(Near(PositionOf(World, Platform).y, 0.0f, 0.001f));  // kinematic: gravity doesn't move it
    }

    void TestForces() {
        std::printf("forces and impulses\n");

        PhysicsWorld World;
        World.SetGravity({0.0f, 0.0f, 0.0f});
        const BodyHandle Box = AddBox(World, {0.0f, 0.0f, 0.0f});

        World.AddImpulse(Box, {5.0f, 0.0f, 0.0f});  // mass 1 -> 5 m/s
        CHECK(Near(World.GetLinearVelocity(Box).x, 5.0f, 0.01f));

        World.SetLinearVelocity(Box, {0.0f, 0.0f, 0.0f});
        for (int i = 0; i < 60; ++i) {
            World.AddForce(Box, {2.0f, 0.0f, 0.0f});
            World.Step(Dt);
        }
        CHECK(World.GetLinearVelocity(Box).x > 1.5f);  // ~2 m/s after a second at 2 N / 1 kg
    }

    void TestCharacter() {
        std::printf("character controller\n");

        PhysicsWorld World;
        AddFloor(World);

        CharacterDesc Desc;
        Desc.Position = {0.0f, 3.0f, 0.0f};
        const CharacterHandle Character = World.CreateCharacter(Desc);
        CHECK(Character != InvalidCharacter);

        // Fall under our own gravity until it lands (the owner integrates it).
        f32 Vertical = 0.0f;
        for (int i = 0; i < 180; ++i) {
            const CharacterState Before = World.GetCharacterState(Character);
            if (Before.Grounded && Vertical <= 0.0f) Vertical = 0.0f;
            else Vertical += World.GetGravity().y * Dt;
            World.SetCharacterVelocity(Character, {0.0f, Vertical, 0.0f});
            World.Step(Dt);
        }
        CharacterState State = World.GetCharacterState(Character);
        CHECK(State.Grounded);
        CHECK(Near(State.Position.y, 0.9f, 0.05f));  // capsule center: half of 1.8 above the floor

        // Walk 2 m/s for a second.
        for (int i = 0; i < 60; ++i) {
            World.SetCharacterVelocity(Character, {2.0f, 0.0f, 0.0f});
            World.Step(Dt);
        }
        State = World.GetCharacterState(Character);
        CHECK(Near(State.Position.x, 2.0f, 0.2f));
        CHECK(State.Grounded);
        CHECK(Near(State.Position.y, 0.9f, 0.05f));

        // A wall at x = 5 (face at 4.5) stops a 4 m/s walk short of it.
        BodyDesc Wall;
        Wall.Motion            = BodyMotion::Static;
        Wall.Shape.Type        = ShapeType::Box;
        Wall.Shape.HalfExtents = {0.5f, 3.0f, 10.0f};
        Wall.Position          = {5.0f, 3.0f, 0.0f};
        CHECK(World.CreateBody(Wall) != InvalidBody);

        for (int i = 0; i < 180; ++i) {
            World.SetCharacterVelocity(Character, {4.0f, 0.0f, 0.0f});
            World.Step(Dt);
        }
        State = World.GetCharacterState(Character);
        CHECK(State.Position.x < 4.2f);   // 4.5 minus the 0.4 radius, plus a little skin
        CHECK(State.Position.x > 3.5f);

        // Teleport.
        World.SetCharacterPosition(Character, {-10.0f, 0.9f, 0.0f});
        CHECK(Near(World.GetCharacterState(Character).Position.x, -10.0f, 0.001f));

        World.DestroyCharacter(Character);
        CHECK(!World.GetCharacterState(Character).Grounded);
    }

    void TestCharacterStepsUp() {
        std::printf("character climbs a step\n");

        PhysicsWorld World;
        AddFloor(World);

        // A 0.3 m ledge (under the default 0.4 m step height).
        BodyDesc Ledge;
        Ledge.Motion            = BodyMotion::Static;
        Ledge.Shape.Type        = ShapeType::Box;
        Ledge.Shape.HalfExtents = {2.0f, 0.15f, 5.0f};
        Ledge.Position          = {4.0f, 0.15f, 0.0f};
        CHECK(World.CreateBody(Ledge) != InvalidBody);

        CharacterDesc Desc;
        Desc.Position                   = {0.0f, 0.9f, 0.0f};
        const CharacterHandle Character = World.CreateCharacter(Desc);

        // 2.5 s at 2 m/s ends at x = 5, in the middle of the ledge (x 2..6).
        for (int i = 0; i < 150; ++i) {
            World.SetCharacterVelocity(Character, {2.0f, 0.0f, 0.0f});
            World.Step(Dt);
        }
        const CharacterState State = World.GetCharacterState(Character);
        std::printf("        (character at x=%.3f y=%.3f)\n", State.Position.x, State.Position.y);
        CHECK(State.Position.x > 4.0f);                 // got onto/over the ledge
        CHECK(Near(State.Position.y, 1.2f, 0.06f));     // standing on top of it (0.3 + 0.9)
    }

    void TestClear() {
        std::printf("clear\n");

        PhysicsWorld World;
        const BodyHandle Floor = AddFloor(World);
        const BodyHandle Box   = AddBox(World, {0.0f, 2.0f, 0.0f});
        const CharacterHandle Character = World.CreateCharacter({});
        CHECK(World.IsValid(Floor));
        CHECK(World.IsValid(Box));

        World.Clear();
        CHECK(!World.IsValid(Floor));
        CHECK(!World.IsValid(Box));
        CHECK(!World.Raycast({0.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 20.0f).Hit);
        CHECK(!World.GetCharacterState(Character).Grounded);

        World.DestroyBody(Floor);  // stale handles are harmless
        Step(World, 5);
    }

    // --- Components through a Scene -----------------------------------------------------------------------------------

    // A scene file with a static floor and a dynamic box, the way the editor
    // saves one. Motion/Shape are the enum values (see RigidBodyComponent).
    std::string TwoBodyScene(const f32 BoxY) {
        const std::string Y = std::to_string(BoxY);
        return R"({
  "Version": 2, "Name": "Physics",
  "Actors": [
    { "ID": 1, "Parent": 0,
      "Properties": { "Name": "Ground", "Enabled": true,
        "Transform": { "Position": [0, -0.5, 0], "Rotation": [0, 0, 0, 1], "Scale": [100, 1, 100] } },
      "Components": [ { "Type": "RigidBodyComponent", "Properties": { "Motion": 0, "Shape": 0 } } ] },
    { "ID": 2, "Parent": 0,
      "Properties": { "Name": "Box", "Enabled": true,
        "Transform": { "Position": [0, )" + Y + R"(, 0], "Rotation": [0, 0, 0, 1], "Scale": [1, 1, 1] } },
      "Components": [ { "Type": "RigidBodyComponent", "Properties": { "Motion": 1, "Shape": 0 } } ] },
    { "ID": 3, "Parent": 0,
      "Properties": { "Name": "Player", "Enabled": true,
        "Transform": { "Position": [20, 3, 0], "Rotation": [0, 0, 0, 1], "Scale": [1, 1, 1] } },
      "Components": [ { "Type": "CharacterControllerComponent", "Properties": {} } ] }
  ]
})";
    }

    void TickScene(Scene& S, PhysicsWorld& World, const int Steps) {
        for (int i = 0; i < Steps; ++i) {
            S.FixedTick(Dt);
            World.Step(Dt);
            S.SyncPhysics();
        }
    }

    void TestComponents() {
        std::printf("components in a scene\n");

        PhysicsWorld World;
        EngineContext Context;
        Context.Physics = &World;

        Scene S("Physics", Context);
        SceneSerializer::LoadFromString(S, TwoBodyScene(4.0f));
        S.BeginPlay();

        const auto Find = [&](const char* Name) -> Actor* {
            Actor* Found = nullptr;
            S.ForEachActor([&](Actor& A) {
                if (A.GetName() == Name) Found = &A;
            });
            return Found;
        };
        Actor* Box    = Find("Box");
        Actor* Player = Find("Player");
        CHECK(Box && Player);
        if (!Box || !Player) return;

        // Dynamic body: the box falls and settles on the (scaled, static) ground, and the actor follows it.
        TickScene(S, World, 240);
        CHECK(Near(Box->GetWorldTransform().Position.y, 0.5f, 0.05f));
        CHECK(Near(Box->GetWorldTransform().Position.x, 0.0f, 0.05f));

        // Moved by game code/the editor: the body teleports to follow, then falls again from there.
        Box->SetPosition({5.0f, 6.0f, 0.0f});
        TickScene(S, World, 2);
        CHECK(Near(Box->GetWorldTransform().Position.x, 5.0f, 0.05f));
        CHECK(Box->GetWorldTransform().Position.y > 5.0f);
        TickScene(S, World, 240);
        CHECK(Near(Box->GetWorldTransform().Position.y, 0.5f, 0.05f));
        CHECK(Near(Box->GetWorldTransform().Position.x, 5.0f, 0.05f));

        // The character falls and stands on the ground (the actor is its capsule center).
        TickScene(S, World, 240);
        CHECK(Near(Player->GetWorldTransform().Position.y, 0.9f, 0.06f));
        CHECK(Near(Player->GetWorldTransform().Position.x, 20.0f, 0.01f));

        // Children follow their parent's motion: a parented body reports its pose in the parent's space.
        // (Covered by Actor::SetWorldPositionAndRotation's round trip below.)

        // Ending play removes every body.
        S.EndPlay();
        CHECK(!World.Raycast({0.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 20.0f).Hit);
    }

    // Frames land between fixed steps. Run Game::TickFrame's loop by hand with frame times that beat against the
    // step (alternating 13.0 / 20.4 ms, a 16.7 ms average: frames alternately get no step and two) and measure how
    // fast a character walking at a constant 5 m/s appears to move from frame to frame.
    struct WalkSpread {
        f32 Min {1e9f};
        f32 Max {-1e9f};
    };

    WalkSpread MeasureWalk(const bool Interpolate) {
        PhysicsWorld World;
        EngineContext Context;
        Context.Physics = &World;

        Scene S("Interp", Context);
        SceneSerializer::LoadFromString(S, TwoBodyScene(4.0f));
        S.BeginPlay();

        Actor* Player = nullptr;
        S.ForEachActor([&](Actor& A) {
            if (A.GetName() == "Player") Player = &A;
        });
        WalkSpread Out;
        if (!Player) return Out;

        TickScene(S, World, 240);  // land
        Player->GetComponent<CharacterControllerComponent>()->SetDesiredVelocity({5.0f, 0.0f, 0.0f});

        const f32 Frames[2] = {0.0130f, 0.0204f};
        f32 Accumulator     = 0.0f;
        f32 PreviousX       = Player->GetWorldTransform().Position.x;

        for (int Frame = 0; Frame < 400; ++Frame) {
            if (Interpolate) S.RestorePhysicsPoses();

            Accumulator += Frames[Frame % 2];
            while (Accumulator >= Dt) {
                S.FixedTick(Dt);
                World.Step(Dt);
                S.SyncPhysics();
                Accumulator -= Dt;
            }

            if (Interpolate) S.ApplyPhysicsInterpolation(Accumulator / Dt);

            const f32 X = Player->GetWorldTransform().Position.x;
            if (Frame >= 40) {  // past the start-up ramp
                const f32 Speed = (X - PreviousX) / Frames[Frame % 2];
                Out.Min         = std::min(Out.Min, Speed);
                Out.Max         = std::max(Out.Max, Speed);
            }
            PreviousX = X;
        }
        return Out;
    }

    void TestInterpolation() {
        std::printf("render interpolation\n");

        const WalkSpread Raw    = MeasureWalk(false);
        const WalkSpread Smooth = MeasureWalk(true);
        std::printf("        (apparent speed, m/s: raw %.2f..%.2f, interpolated %.2f..%.2f)\n",
                    Raw.Min, Raw.Max, Smooth.Min, Smooth.Max);

        // Unsmoothed, a frame with no step doesn't move and one with two covers twice the ground.
        CHECK(Raw.Max - Raw.Min > 3.0f);
        // Smoothed, it moves at the real speed every frame.
        CHECK(Smooth.Min > 4.7f);
        CHECK(Smooth.Max < 5.3f);

        // The simulation sees the true pose, not the blended one, and a move made in between sticks.
        PhysicsWorld World;
        EngineContext Context;
        Context.Physics = &World;
        Scene S("Interp2", Context);
        SceneSerializer::LoadFromString(S, TwoBodyScene(4.0f));
        S.BeginPlay();

        Actor* Box = nullptr;
        S.ForEachActor([&](Actor& A) {
            if (A.GetName() == "Box") Box = &A;
        });
        CHECK(Box != nullptr);
        if (!Box) return;

        TickScene(S, World, 20);  // falling, so successive poses differ
        const f32 SimulatedY = Box->GetWorldTransform().Position.y;
        S.ApplyPhysicsInterpolation(0.5f);
        const f32 BlendedY = Box->GetWorldTransform().Position.y;
        CHECK(BlendedY > SimulatedY);  // halfway between the previous (higher) pose and the current
        S.RestorePhysicsPoses();
        CHECK(Near(Box->GetWorldTransform().Position.y, SimulatedY, 1e-5f));

        S.ApplyPhysicsInterpolation(0.5f);
        Box->SetPosition({9.0f, 12.0f, 0.0f});  // game code moves it after it was drawn
        S.RestorePhysicsPoses();
        CHECK(Near(Box->GetWorldTransform().Position.x, 9.0f, 1e-5f));  // not overwritten by the restore
        TickScene(S, World, 2);
        CHECK(Near(Box->GetWorldTransform().Position.x, 9.0f, 0.05f));  // the body followed
        CHECK(Box->GetWorldTransform().Position.y > 10.0f);
    }

    void TestWorldTransformRoundTrip() {
        std::printf("Actor::SetWorldPositionAndRotation under a parent\n");

        Scene S("Parenting");
        const ActorHandle ParentHandle = S.Spawn("Parent");
        const ActorHandle ChildHandle  = S.Spawn("Child");
        Actor* Parent                  = S.Get(ParentHandle);
        Actor* Child                   = S.Get(ChildHandle);

        Parent->SetPosition({10.0f, 2.0f, -3.0f});
        Parent->SetScale({2.0f, 2.0f, 2.0f});
        Quat Turn;
        DirectX::XMStoreFloat4(&Turn, DirectX::XMQuaternionRotationRollPitchYaw(0.0f, 1.0f, 0.0f));
        Parent->SetRotation(Turn);
        Child->AttachTo(ParentHandle);

        Quat Target;
        DirectX::XMStoreFloat4(&Target, DirectX::XMQuaternionRotationRollPitchYaw(0.3f, -0.4f, 0.2f));
        Child->SetWorldPositionAndRotation({1.0f, 5.0f, 7.0f}, Target);

        const Transform World = Child->GetWorldTransform();
        CHECK(Near(World.Position.x, 1.0f, 1e-3f));
        CHECK(Near(World.Position.y, 5.0f, 1e-3f));
        CHECK(Near(World.Position.z, 7.0f, 1e-3f));
        CHECK(Near(std::abs(World.Rotation.x * Target.x + World.Rotation.y * Target.y + World.Rotation.z * Target.z +
                            World.Rotation.w * Target.w),
                   1.0f,
                   1e-4f));
    }
}  // namespace

int main() {
    TestFall();
    TestRestsOnFloor();
    TestMeshFloor();
    TestRaycast();
    TestTeleportAndKinematic();
    TestForces();
    TestCharacter();
    TestCharacterStepsUp();
    TestClear();
    TestComponents();
    TestInterpolation();
    TestWorldTransformRoundTrip();

    std::printf("\n%d checks, %d failed\n", Checks, Failures);
    return Failures == 0 ? 0 : 1;
}
