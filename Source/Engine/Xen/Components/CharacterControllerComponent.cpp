//
// Created by Jake Rieger on 10/6/2026.
//

#include "CharacterControllerComponent.hpp"

#include <Common/Log.hpp>
#include <Xen/Actor.hpp>
#include <Xen/Scene.hpp>

#include <cmath>

namespace Xen {
    void CharacterControllerComponent::Reflect(IReflector& R) {
        R.Property("Radius", _Radius, {.Category = "Character", .Min = 0.05f, .Max = 10.0f});
        R.Property("Height",
                   _Height,
                   {.ToolTip = "Total height including both end caps.", .Category = "Character", .Min = 0.1f, .Max = 20.0f});
        R.Property("StepHeight",
                   _StepHeight,
                   {.ToolTip = "The tallest ledge the character walks up onto.", .Category = "Character", .Min = 0.0f, .Max = 5.0f});
        R.Property("MaxSlope",
                   _MaxSlopeDegrees,
                   {.ToolTip  = "Steepest slope, in degrees, the character can stand on.",
                    .Category = "Character",
                    .Min      = 0.0f,
                    .Max      = 89.0f});
        R.Property("Mass",
                   _Mass,
                   {.ToolTip = "How hard the character pushes dynamic bodies it walks into.", .Category = "Character", .Min = 1.0f, .Max = 10000.0f});
        R.Property("UseGravity", _UseGravity, {.Category = "Character"});
        // The handle, velocities and grounded flag are runtime state.
    }

    PhysicsWorld* CharacterControllerComponent::GetWorld() const {
        const Scene* S = GetScene();
        return S ? S->GetContext().Physics : nullptr;
    }

    void CharacterControllerComponent::BeginPlay() {
        PhysicsWorld* World = GetWorld();
        if (!World) return;

        const Transform Pose = GetOwner()->GetWorldTransform();

        CharacterDesc Desc;
        Desc.Radius          = _Radius;
        Desc.Height          = _Height;
        Desc.StepHeight      = _StepHeight;
        Desc.MaxSlopeDegrees = _MaxSlopeDegrees;
        Desc.Mass            = _Mass;
        Desc.Position        = Pose.Position;
        Desc.Rotation        = Pose.Rotation;

        _Character = World->CreateCharacter(Desc);
        if (_Character == InvalidCharacter) {
            LOG_ERR("CharacterController on '%s' couldn't create its character", GetOwner()->GetName().c_str());
            return;
        }

        _LastPosition = _PrevPosition = _CurrPosition = Pose.Position;
        _Interpolated     = false;
        _VerticalVelocity = 0.0f;
        _Grounded         = false;
    }

    void CharacterControllerComponent::EndPlay() {
        if (_Character == InvalidCharacter) return;

        if (PhysicsWorld* World = GetWorld()) World->DestroyCharacter(_Character);
        _Character = InvalidCharacter;
    }

    void CharacterControllerComponent::SetDesiredVelocity(const Float3& Velocity) {
        _DesiredVelocity = {Velocity.x, 0.0f, Velocity.z};
    }

    void CharacterControllerComponent::Jump(const f32 Speed) {
        if (!_Grounded) return;
        _JumpRequested    = true;
        _PendingJumpSpeed = Speed;
    }

    void CharacterControllerComponent::FixedTick(const f32 FixedDelta) {
        PhysicsWorld* World = GetWorld();
        if (!World || _Character == InvalidCharacter) return;

        // Moved by the editor or game code, not by the simulation.
        const Float3 Position = GetOwner()->GetWorldTransform().Position;
        if (std::abs(Position.x - _LastPosition.x) > 1e-5f || std::abs(Position.y - _LastPosition.y) > 1e-5f ||
            std::abs(Position.z - _LastPosition.z) > 1e-5f) {
            World->SetCharacterPosition(_Character, Position);
            _PrevPosition = _CurrPosition = Position;  // a teleport, not motion to smooth over
            _VerticalVelocity = 0.0f;
        }
        _LastPosition = Position;

        if (_JumpRequested && _Grounded) {
            _VerticalVelocity = _PendingJumpSpeed;
        } else if (!_UseGravity) {
            _VerticalVelocity = 0.0f;
        } else if (_Grounded && _VerticalVelocity <= 0.0f) {
            // Standing on something: no accumulated fall speed.
            _VerticalVelocity = 0.0f;
        } else {
            _VerticalVelocity += World->GetGravity().y * FixedDelta;
        }
        _JumpRequested = false;

        World->SetCharacterVelocity(_Character, {_DesiredVelocity.x, _VerticalVelocity, _DesiredVelocity.z});
    }

    void CharacterControllerComponent::PullFromPhysics() {
        PhysicsWorld* World = GetWorld();
        if (!World || _Character == InvalidCharacter) return;

        const CharacterState State = World->GetCharacterState(_Character);
        _Grounded                  = State.Grounded;
        _PrevPosition              = _CurrPosition;
        _CurrPosition              = State.Position;

        Actor* Owner = GetOwner();
        Owner->SetWorldPositionAndRotation(State.Position, Owner->GetWorldTransform().Rotation);

        // Read back rather than assume - see RigidBodyComponent::PullFromPhysics.
        _LastPosition = Owner->GetWorldTransform().Position;
    }

    namespace {
        bool PositionDiffers(const Float3& A, const Float3& B) {
            return std::abs(A.x - B.x) > 1e-5f || std::abs(A.y - B.y) > 1e-5f || std::abs(A.z - B.z) > 1e-5f;
        }
    }  // namespace

    void CharacterControllerComponent::ApplyInterpolation(const f32 Alpha) {
        if (_Character == InvalidCharacter || !PositionDiffers(_PrevPosition, _CurrPosition)) return;

        const f32 T = Alpha < 0.0f ? 0.0f : (Alpha > 1.0f ? 1.0f : Alpha);
        const Float3 Position {_PrevPosition.x + (_CurrPosition.x - _PrevPosition.x) * T,
                               _PrevPosition.y + (_CurrPosition.y - _PrevPosition.y) * T,
                               _PrevPosition.z + (_CurrPosition.z - _PrevPosition.z) * T};

        Actor* Owner = GetOwner();
        Owner->SetWorldPositionAndRotation(Position, Owner->GetWorldTransform().Rotation);
        _AppliedPosition = Owner->GetWorldTransform().Position;
        _Interpolated    = true;
    }

    void CharacterControllerComponent::RestoreSimulatedPose() {
        if (!_Interpolated) return;
        _Interpolated = false;

        Actor* Owner = GetOwner();
        // Moved by game code since it was drawn: leave it where it was put.
        // FixedTick sees it differs from the simulated position and teleports.
        if (PositionDiffers(Owner->GetWorldTransform().Position, _AppliedPosition)) return;

        Owner->SetWorldPositionAndRotation(_CurrPosition, Owner->GetWorldTransform().Rotation);
        _LastPosition = Owner->GetWorldTransform().Position;
    }
}  // namespace Xen
