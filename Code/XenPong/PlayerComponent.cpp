//
// Created by Jake Rieger on 9/8/2026.
//

#include "PlayerComponent.hpp"

#include <Engine/Scene.hpp>
#include <Engine/Actor.hpp>

using namespace Xen;

void PlayerComponent::Reflect(IReflector& R) {}

void PlayerComponent::BeginPlay() {
    Reset();
}

void PlayerComponent::Tick(f32 DeltaTime) {}

void PlayerComponent::FixedTick(f32 FixedDelta) {}

void PlayerComponent::EndPlay() {}

void PlayerComponent::Reset() {
    auto MainCamera = GetOwner()->GetScene()->GetMainCamera();
    auto BoundsX    = MainCamera->GetViewportWidth() / MainCamera->GetPixelsPerUnit();
    auto BoundsY    = MainCamera->GetViewportHeight() / MainCamera->GetPixelsPerUnit();

    auto PosX = -(BoundsX / 2.1f);
    auto PosY = BoundsY * 0.5f;

    GetOwner()->SetPosition(glm::vec2(PosX, PosY));
}