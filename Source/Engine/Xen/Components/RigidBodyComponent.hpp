//
// Created by Jake Rieger on 10/6/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include <Xen/Component.hpp>
#include <Xen/ComponentRegistry.hpp>
#include <Xen/Physics/PhysicsWorld.hpp>

namespace Xen {
    XEN_COMPONENT(RigidBodyComponent)

    /// @brief Gives an actor a physics body and a collision shape.
    ///
    /// Static bodies never move (floors, walls); Dynamic bodies are simulated
    /// (gravity, collisions, forces) and drive their actor's transform;
    /// Kinematic bodies follow their actor's transform and push dynamic ones.
    ///
    /// The body is created in BeginPlay from the actor's world transform and
    /// scale, and destroyed in EndPlay. It only simulates while the game's
    /// simulation is enabled, so in the editor it just sits where the actor
    /// is; moving the actor (in the editor, or from game code) teleports the
    /// body on the next fixed step. Scale changes after BeginPlay don't
    /// resize the shape.
    ///
    /// A shape size of zero means "fit the actor's MeshComponent", so a body
    /// next to a mesh needs no sizing to match it.
    class RigidBodyComponent final : public IComponent {
    public:
        XEN_COMPONENT_STATICS(RigidBodyComponent)
        RigidBodyComponent() = default;

        void Reflect(IReflector& R) override;
        void BeginPlay() override;
        void EndPlay() override;
        void FixedTick(f32 FixedDelta) override;

        /// @brief Reads the simulated pose back into the actor. Called by
        /// Scene::SyncPhysics after each physics step.
        void PullFromPhysics();

        /// @brief Render smoothing. The simulation advances in fixed steps, but
        /// frames land between them, so the actor is shown at a blend of the
        /// last two simulated poses (Alpha = how far into the next step the
        /// frame is) - otherwise motion visibly judders whenever the frame and
        /// step rates don't line up. Game applies it just before rendering and
        /// restores the true simulated pose before the next step, so game
        /// code never sees the blended pose during FixedTick; an actor moved
        /// by game code in between is left where it was put.
        void ApplyInterpolation(f32 Alpha);
        void RestoreSimulatedPose();

        NODISCARD bool HasBody() const { return _Body != InvalidBody; }
        NODISCARD BodyHandle GetBody() const { return _Body; }

        NODISCARD BodyMotion GetMotion() const { return _Motion; }

        // Runtime control of a Dynamic body. No-ops without a body.
        NODISCARD Float3 GetLinearVelocity() const;
        void SetLinearVelocity(const Float3& Velocity);
        void AddForce(const Float3& Force);
        void AddImpulse(const Float3& Impulse);

    private:
        PhysicsWorld* GetWorld() const;
        BodyDesc BuildDesc() const;

        BodyMotion _Motion {BodyMotion::Dynamic};
        ShapeType _Shape {ShapeType::Box};

        /// Box: full size on each axis. Zero on any axis = fit the mesh.
        Float3 _Size {0.0f, 0.0f, 0.0f};
        /// Sphere/Capsule radius. Zero = fit the mesh.
        f32 _Radius {0.0f};
        /// Capsule total height. Zero = fit the mesh.
        f32 _Height {0.0f};
        Float3 _Offset {0.0f, 0.0f, 0.0f};
        /// Mesh shape source. Unset = the actor's MeshComponent's mesh.
        AssetID _MeshAsset {};

        f32 _Mass {1.0f};
        f32 _Friction {0.5f};
        f32 _Restitution {0.0f};
        f32 _LinearDamping {0.05f};
        f32 _AngularDamping {0.05f};
        f32 _GravityScale {1.0f};

        // Runtime only - never reflected.
        BodyHandle _Body {InvalidBody};
        // The pose last written to / read from the actor, to tell a move made
        // by someone else (editor, game code) from the simulation's own.
        Float3 _LastPosition {0.0f, 0.0f, 0.0f};
        Quat _LastRotation {IdentityQuat};

        // The last two simulated world poses, and the blended pose currently
        // shown (see ApplyInterpolation).
        Float3 _PrevPosition {0.0f, 0.0f, 0.0f};
        Quat _PrevRotation {IdentityQuat};
        Float3 _CurrPosition {0.0f, 0.0f, 0.0f};
        Quat _CurrRotation {IdentityQuat};
        bool _Interpolated {false};
        Float3 _AppliedPosition {0.0f, 0.0f, 0.0f};
        Quat _AppliedRotation {IdentityQuat};
    };
}  // namespace Xen
