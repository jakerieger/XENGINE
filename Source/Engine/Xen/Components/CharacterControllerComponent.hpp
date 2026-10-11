//
// Created by Jake Rieger on 10/6/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include <Xen/Component.hpp>
#include <Xen/ComponentRegistry.hpp>
#include <Xen/Physics/PhysicsWorld.hpp>

namespace Xen {
    XEN_COMPONENT(CharacterControllerComponent)

    /// @brief A walking capsule: slides along walls, climbs steps, handles
    /// slopes and falls under gravity. The backbone of a player controller.
    ///
    /// Game code drives it with SetDesiredVelocity (horizontal movement) and
    /// Jump; the component integrates gravity itself and, after each physics
    /// step, moves the actor to where the capsule ended up. The actor's
    /// position is the capsule's center, and its rotation is left alone (the
    /// capsule always stays upright), so a camera on the same actor looks
    /// from the middle of the body.
    ///
    /// Like RigidBodyComponent, the capsule is created in BeginPlay and only
    /// moves while the simulation is enabled; moving the actor from outside
    /// teleports the capsule.
    class CharacterControllerComponent final : public IComponent {
    public:
        XEN_COMPONENT_STATICS(CharacterControllerComponent)
        CharacterControllerComponent() = default;

        void Reflect(IReflector& R) override;
        void BeginPlay() override;
        void EndPlay() override;
        void FixedTick(f32 FixedDelta) override;

        /// @brief Reads the capsule's new position back into the actor.
        /// Called by Scene::SyncPhysics after each physics step.
        void PullFromPhysics();

        /// @brief Render smoothing, as RigidBodyComponent::ApplyInterpolation:
        /// the actor is shown between its last two simulated positions so that
        /// walking doesn't judder when frames and fixed steps don't line up.
        /// Position only - rotation is the owner's (mouse look runs per frame).
        void ApplyInterpolation(f32 Alpha);
        void RestoreSimulatedPose();

        /// @brief Horizontal velocity in world space, meters per second.
        /// Y is ignored (gravity and Jump own the vertical axis). Stays in
        /// effect until changed, so set it every fixed step from input.
        void SetDesiredVelocity(const Float3& Velocity);

        /// @brief Launches the character upward at Speed m/s if it is
        /// standing on something. A call while airborne is dropped.
        void Jump(f32 Speed);

        NODISCARD bool IsGrounded() const { return _Grounded; }

        /// @brief Horizontal desired velocity plus the current vertical speed.
        NODISCARD Float3 GetVelocity() const { return {_DesiredVelocity.x, _VerticalVelocity, _DesiredVelocity.z}; }

        NODISCARD bool HasCharacter() const { return _Character != InvalidCharacter; }

    private:
        PhysicsWorld* GetWorld() const;

        f32 _Radius {0.4f};
        f32 _Height {1.8f};
        f32 _StepHeight {0.4f};
        f32 _MaxSlopeDegrees {50.0f};
        f32 _Mass {70.0f};
        bool _UseGravity {true};

        // Runtime only - never reflected.
        CharacterHandle _Character {InvalidCharacter};
        Float3 _DesiredVelocity {0.0f, 0.0f, 0.0f};
        f32 _VerticalVelocity {0.0f};
        f32 _PendingJumpSpeed {0.0f};
        bool _JumpRequested {false};
        bool _Grounded {false};
        Float3 _LastPosition {0.0f, 0.0f, 0.0f};
        Float3 _PrevPosition {0.0f, 0.0f, 0.0f};
        Float3 _CurrPosition {0.0f, 0.0f, 0.0f};
        bool _Interpolated {false};
        Float3 _AppliedPosition {0.0f, 0.0f, 0.0f};
    };
}  // namespace Xen
