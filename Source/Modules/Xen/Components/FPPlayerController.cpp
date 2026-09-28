//
// Created by Jake Rieger on 9/20/2026.
//

#include "FPPlayerController.hpp"
#include "CameraComponent.hpp"
#include "Actor.hpp"

namespace Xen {
    FPPlayerController::FPPlayerController() {
        _Camera = GetOwner()->AddComponent<CameraComponent>();
        if (!_Camera) { THROW_ENGINE_EXCEPTION(EngineException, "Failed to create camera component"); }

        _Camera->SetProjectionMode(ProjectionMode::Perspective);
        _Camera->SetFieldOfView(70.0f);
    }

    FPPlayerController::~FPPlayerController() {}

    void FPPlayerController::Reflect(IReflector& R) {}

    void FPPlayerController::BeginPlay() {}

    void FPPlayerController::Tick(const f32 DeltaTime) {}

    void FPPlayerController::FixedTick(const f32 FixedDelta) {}

    void FPPlayerController::EndPlay() {}
}  // namespace Xen