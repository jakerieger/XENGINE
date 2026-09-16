//
// Created by Jake Rieger on 9/8/2026.
//

#include "OpponentComponent.hpp"

#include <Xen/Scene.hpp>
#include <Xen/Actor.hpp>

namespace Xen {
    void OpponentComponent::Reflect(IReflector& R) {}

    void OpponentComponent::BeginPlay() {
        Reset();
    }

    void OpponentComponent::Tick(f32 DeltaTime) {}

    void OpponentComponent::EndPlay() {}

    void OpponentComponent::Reset() {
        const auto MainCamera = GetOwner()->GetScene()->GetMainCamera();
        const auto BoundsX    = MainCamera->GetViewportWidth() / MainCamera->GetPixelsPerUnit();

        // Mirror of PlayerComponent::Reset() - just inboard of the right edge,
        // vertically centered.
        const auto PosX = BoundsX / 2.1f;

        GetOwner()->SetPosition(Float2(PosX, 0.0f));
    }

}  // namespace Xen