//
// Created by Jake Rieger on 9/20/2026.
//

#pragma once

#include "ComponentRegistry.hpp"

namespace Xen {
    class CameraComponent;

    REGISTER_COMPONENT(FPPlayerController)
    class FPPlayerController : public IComponent {
    public:
        XEN_COMPONENT_TYPE(FPPlayerController)
        FPPlayerController();
        ~FPPlayerController() override;

        void Reflect(IReflector& R) override;

        void BeginPlay() override;

        void Tick(f32 DeltaTime) override;

        void FixedTick(f32 FixedDelta) override;

        void EndPlay() override;

    private:
        CameraComponent* _Camera {nullptr};
    };
}  // namespace Xen
