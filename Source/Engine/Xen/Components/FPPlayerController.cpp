//
// Created by Jake Rieger on 9/20/2026.
//

#include "FPPlayerController.hpp"
#include "CharacterControllerComponent.hpp"
#include <Xen/Actor.hpp>
#include <Xen/Game.hpp>

#include <Common/Log.hpp>

#include <algorithm>
#include <cmath>

namespace Xen {
    namespace {
        // Just short of straight up/down, where yaw and pitch stop being
        // independent.
        constexpr f32 MaxPitch = DirectX::XMConvertToRadians(89.0f);
    }  // namespace

    void FPPlayerController::Reflect(IReflector& R) {
        R.Property("MoveSpeed", _MoveSpeed, {.ToolTip = "Meters per second.", .Category = "Movement", .Min = 0.0f, .Max = 100.0f});
        R.Property("SprintMultiplier",
                   _SprintMultiplier,
                   {.ToolTip = "Speed multiplier while Shift is held.", .Category = "Movement", .Min = 1.0f, .Max = 10.0f});
        R.Property("JumpSpeed",
                   _JumpSpeed,
                   {.ToolTip = "Launch speed in meters per second.", .Category = "Movement", .Min = 0.0f, .Max = 50.0f});
        R.Property("MouseSensitivity",
                   _MouseSensitivity,
                   {.ToolTip = "Radians of turn per mouse count.", .Category = "Look", .Min = 0.0001f, .Max = 0.05f});
        R.Property("LockCursor",
                   _LockCursor,
                   {.ToolTip  = "Lock and hide the cursor at the middle of the game window while playing.",
                    .Category = "Look"});
    }

    void FPPlayerController::BeginPlay() {
        _Character = GetOwner()->GetComponent<CharacterControllerComponent>();
        if (!_Character) {
            LOG_WARN("FPPlayerController on '%s' has no CharacterControllerComponent - it can't move",
                     GetOwner()->GetName().c_str());
        }

        // Start looking where the actor already faces.
        using namespace DirectX;
        const Quat Rotation = GetOwner()->GetWorldTransform().Rotation;
        XMFLOAT3 Forward;
        XMStoreFloat3(&Forward, XMVector3Rotate(XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), XMLoadFloat4(&Rotation)));
        _Yaw   = std::atan2(-Forward.x, -Forward.z);
        _Pitch = std::asin(std::clamp(Forward.y, -1.0f, 1.0f));

        if (_LockCursor) {
            if (const Game* G = GetGame()) {
                G->SetCursorLocked(true);
                _LockedCursor = G->IsCursorLocked();
            }
        }
    }

    void FPPlayerController::Tick(const f32 DeltaTime) {
        (void)DeltaTime;

        const Game* G = GetGame();
        if (!G) return;
        const InputManager& Input = G->GetInputManager();

        // Mouse right turns right (negative yaw about +Y), mouse down looks down.
        _Yaw -= CAST<f32>(Input.GetMouseDeltaX()) * _MouseSensitivity;
        _Pitch = std::clamp(_Pitch - CAST<f32>(Input.GetMouseDeltaY()) * _MouseSensitivity, -MaxPitch, MaxPitch);

        // Pitch about the local X axis, then yaw about world Y.
        using namespace DirectX;
        Quat Rotation;
        XMStoreFloat4(&Rotation, XMQuaternionRotationRollPitchYaw(_Pitch, _Yaw, 0.0f));
        GetOwner()->SetRotation(Rotation);

        if (Input.WasKeyPressed(Input::KeyCode::Space)) _JumpQueued = true;
    }

    void FPPlayerController::FixedTick(const f32 FixedDelta) {
        (void)FixedDelta;
        if (!_Character) return;

        const Game* G = GetGame();
        if (!G) return;
        const InputManager& Input = G->GetInputManager();

        f32 Forward = 0.0f;
        f32 Right   = 0.0f;
        if (Input.GetKeyDown(Input::KeyCode::W)) Forward += 1.0f;
        if (Input.GetKeyDown(Input::KeyCode::S)) Forward -= 1.0f;
        if (Input.GetKeyDown(Input::KeyCode::D)) Right += 1.0f;
        if (Input.GetKeyDown(Input::KeyCode::A)) Right -= 1.0f;

        // Facing -Z at zero yaw; yaw turns counter-clockwise seen from above.
        const f32 SinYaw = std::sin(_Yaw);
        const f32 CosYaw = std::cos(_Yaw);
        Float3 Direction {-SinYaw * Forward + CosYaw * Right, 0.0f, -CosYaw * Forward - SinYaw * Right};

        const f32 Length = std::sqrt(Direction.x * Direction.x + Direction.z * Direction.z);
        if (Length > 1.0f) {
            Direction.x /= Length;
            Direction.z /= Length;
        }

        const f32 Speed = _MoveSpeed * (Input.GetKeyDown(Input::KeyCode::LeftShift) ? _SprintMultiplier : 1.0f);
        _Character->SetDesiredVelocity({Direction.x * Speed, 0.0f, Direction.z * Speed});

        if (_JumpQueued) {
            _Character->Jump(_JumpSpeed);
            _JumpQueued = false;
        }
    }

    void FPPlayerController::EndPlay() {
        _Character = nullptr;

        if (_LockedCursor) {
            if (const Game* G = GetGame()) G->SetCursorLocked(false);
            _LockedCursor = false;
        }
    }
}  // namespace Xen
