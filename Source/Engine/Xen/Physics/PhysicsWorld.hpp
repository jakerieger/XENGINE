//
// Created by Jake Rieger on 10/6/2026.
//
// The engine's physics: a thin wrapper over Jolt Physics. Jolt itself stays an
// implementation detail of PhysicsWorld.cpp - nothing in this header (or any
// installed header) mentions a Jolt type, so a game never needs Jolt to build
// against the engine. Bodies and characters are referred to by opaque u32
// handles.
//
// Update order, per fixed step (see Game::TickFrame):
//   1. Scene::FixedTick   - gameplay sets velocities/forces, character input
//   2. PhysicsWorld::Step - simulation, then characters
//   3. Scene::SyncPhysics - components read the results back into their actors
//
// The world is stepped only while the game's simulation is enabled, so a scene
// being edited in XED holds bodies (created in BeginPlay) that never move.
//
// Conventions: right-handed, Y up, meters/seconds/radians - the same as the
// renderer, so transforms pass through unchanged.

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Math.hpp>

#include <memory>
#include <vector>

namespace Xen {
    using BodyHandle                     = u32;
    using CharacterHandle                = u32;
    constexpr BodyHandle InvalidBody     = 0xFFFFFFFFu;
    constexpr CharacterHandle InvalidCharacter = 0xFFFFFFFFu;

    enum class BodyMotion : u8 {
        /// Never moves (floors, walls). Cheapest; can use a mesh shape.
        Static,
        /// Simulated: gravity, forces, collisions.
        Dynamic,
        /// Moved by code (MoveKinematic), pushes dynamic bodies but is never pushed.
        Kinematic,
    };

    enum class ShapeType : u8 { Box, Sphere, Capsule, Mesh };

    struct ShapeDesc {
        ShapeType Type {ShapeType::Box};

        /// Box: half the size on each axis.
        Float3 HalfExtents {0.5f, 0.5f, 0.5f};
        /// Sphere/Capsule.
        f32 Radius {0.5f};
        /// Capsule: total height including both end caps, so it is never
        /// shorter than 2 * Radius.
        f32 Height {2.0f};

        /// Shape center relative to the body's origin.
        Float3 Offset {0.0f, 0.0f, 0.0f};

        /// Mesh: triangle soup (indices in groups of three). Static bodies only.
        std::vector<Float3> Vertices;
        std::vector<u32> Indices;
    };

    struct BodyDesc {
        ShapeDesc Shape;
        BodyMotion Motion {BodyMotion::Dynamic};

        Float3 Position {0.0f, 0.0f, 0.0f};
        Quat Rotation {IdentityQuat};

        f32 Mass {1.0f};  // Dynamic only
        f32 Friction {0.5f};
        f32 Restitution {0.0f};
        f32 LinearDamping {0.05f};
        f32 AngularDamping {0.05f};
        f32 GravityScale {1.0f};
    };

    struct RaycastHit {
        bool Hit {false};
        f32 Distance {0.0f};
        Float3 Point {0.0f, 0.0f, 0.0f};
        Float3 Normal {0.0f, 1.0f, 0.0f};
        BodyHandle Body {InvalidBody};
    };

    struct CharacterDesc {
        f32 Radius {0.4f};
        /// Total height including both end caps.
        f32 Height {1.8f};
        /// Largest ledge the character steps up onto.
        f32 StepHeight {0.4f};
        f32 MaxSlopeDegrees {50.0f};
        /// How hard the character pushes dynamic bodies it walks into.
        f32 Mass {70.0f};

        /// The capsule's center.
        Float3 Position {0.0f, 0.0f, 0.0f};
        /// Yaw only matters for the capsule's orientation in queries; the
        /// capsule always stays upright.
        Quat Rotation {IdentityQuat};
    };

    struct CharacterState {
        /// The capsule's center after the last Step.
        Float3 Position {0.0f, 0.0f, 0.0f};
        /// The velocity the character actually moved with (after collision
        /// response) - zeroed along a blocked axis, e.g. vertical on landing.
        Float3 Velocity {0.0f, 0.0f, 0.0f};
        bool Grounded {false};
    };

    class PhysicsWorld {
    public:
        PhysicsWorld();
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&)            = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;

        // --- Simulation ---------------------------------------------------------------------------------------------

        /// @brief Advances the simulation by DeltaTime seconds, then moves
        /// every character by the velocity last set on it.
        void Step(f32 DeltaTime);

        void SetGravity(const Float3& Gravity);
        NODISCARD Float3 GetGravity() const;

        /// @brief Destroys every body and character. Handles from before the
        /// call are invalid afterward.
        void Clear();

        // --- Bodies -------------------------------------------------------------------------------------------------

        /// @brief Creates a body and adds it to the world. InvalidBody (with
        /// the reason logged) if the shape is unusable, e.g. a Mesh shape on
        /// a non-Static body or one with no triangles.
        NODISCARD BodyHandle CreateBody(const BodyDesc& Desc);
        void DestroyBody(BodyHandle Body);
        NODISCARD bool IsValid(BodyHandle Body) const;

        /// @brief The body's current pose (world space).
        void GetBodyTransform(BodyHandle Body, Float3& Position, Quat& Rotation) const;

        /// @brief Teleports a body, clearing its velocity.
        void SetBodyTransform(BodyHandle Body, const Float3& Position, const Quat& Rotation);

        /// @brief Moves a Kinematic body to a pose over DeltaTime, so it
        /// carries/pushes other bodies with the matching velocity.
        void MoveKinematic(BodyHandle Body, const Float3& Position, const Quat& Rotation, f32 DeltaTime);

        NODISCARD Float3 GetLinearVelocity(BodyHandle Body) const;
        void SetLinearVelocity(BodyHandle Body, const Float3& Velocity);
        void AddForce(BodyHandle Body, const Float3& Force);
        void AddImpulse(BodyHandle Body, const Float3& Impulse);

        /// @brief Casts a ray from Origin along Direction (normalized
        /// internally) up to MaxDistance. Ignore skips one body, e.g. the
        /// caster's own.
        NODISCARD RaycastHit
        Raycast(const Float3& Origin, const Float3& Direction, f32 MaxDistance, BodyHandle Ignore = InvalidBody) const;

        // --- Characters ---------------------------------------------------------------------------------------------

        /// @brief A kinematic capsule that slides along geometry, climbs
        /// steps and handles slopes - the player controller's backbone. It
        /// does not apply gravity itself: the owner integrates it into the
        /// velocity it passes to SetCharacterVelocity.
        NODISCARD CharacterHandle CreateCharacter(const CharacterDesc& Desc);
        void DestroyCharacter(CharacterHandle Character);

        /// @brief The velocity the character moves with on every Step until
        /// changed.
        void SetCharacterVelocity(CharacterHandle Character, const Float3& Velocity);
        void SetCharacterPosition(CharacterHandle Character, const Float3& Position);
        NODISCARD CharacterState GetCharacterState(CharacterHandle Character) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> _Impl;
    };
}  // namespace Xen
