//
// Created by Jake Rieger on 9/20/2026.
//

#pragma once

#include <Xen/ComponentRegistry.hpp>

namespace Xen {
    class CharacterControllerComponent;

    /// @brief First-person walking: WASD to move, Shift to sprint, Space to
    /// jump, mouse to look.
    ///
    /// Drives a CharacterControllerComponent on the same actor (add one - and
    /// a CameraComponent, to see anything) and turns the actor to face where
    /// the mouse points, so a camera on the same actor is the player's view.
    /// Movement is read on the fixed step, looking and jump presses every
    /// frame. Locks and hides the cursor at the middle of the game window while
    /// it is active (see LockCursor), so looking around never drags it out of
    /// the window; that's a no-op inside the editor, which owns its cursor.
    XEN_COMPONENT(FPPlayerController)
    class FPPlayerController : public IComponent {
    public:
        XEN_COMPONENT_STATICS(FPPlayerController)
        FPPlayerController() = default;
        ~FPPlayerController() override = default;

        void Reflect(IReflector& R) override;

        void BeginPlay() override;

        void Tick(f32 DeltaTime) override;

        void FixedTick(f32 FixedDelta) override;

        void EndPlay() override;

    private:
        f32 _MoveSpeed {5.0f};
        f32 _SprintMultiplier {1.6f};
        f32 _JumpSpeed {5.5f};
        /// Radians of turn per mouse count.
        f32 _MouseSensitivity {0.0022f};
        bool _LockCursor {true};
        bool _LockedCursor {false};  // runtime: whether this controller is the one holding the lock

        CharacterControllerComponent* _Character {nullptr};
        f32 _Yaw {0.0f};
        f32 _Pitch {0.0f};
        bool _JumpQueued {false};
    };
}  // namespace Xen
