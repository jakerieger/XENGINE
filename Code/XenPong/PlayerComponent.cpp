//
// Created by Jake Rieger on 9/8/2026.
//

#include "PlayerComponent.hpp"

#include "Game.hpp"

#include <Xen/Scene.hpp>
#include <Xen/Actor.hpp>

using namespace Xen;

void PlayerComponent::Reflect(IReflector& R) {}

void PlayerComponent::BeginPlay() {
    Reset();
}

void PlayerComponent::Tick(f32 DeltaTime) {}

void PlayerComponent::FixedTick(f32 FixedDelta) {
    const auto CurrentPos = GetOwner()->GetPosition();

    if (GetGame()->GetInputManager().GetKeyDown(Input::KeyCode::Up)) {
        const auto PosY = CurrentPos.y + 8.f * FixedDelta;
        GetOwner()->SetPosition({CurrentPos.x, PosY});
    }

    if (GetGame()->GetInputManager().GetKeyDown(Input::KeyCode::Down)) {
        const auto PosY = CurrentPos.y - 8.f * FixedDelta;
        GetOwner()->SetPosition({CurrentPos.x, PosY});
    }
}

void PlayerComponent::EndPlay() {}

void PlayerComponent::Reset() {
    auto MainCamera = GetOwner()->GetScene()->GetMainCamera();
    auto BoundsX    = MainCamera->GetViewportWidth() / MainCamera->GetPixelsPerUnit();
    auto BoundsY    = MainCamera->GetViewportHeight() / MainCamera->GetPixelsPerUnit();

    auto PosX = -(BoundsX / 2.1f);
    auto PosY = BoundsY * 0.5f;

    GetOwner()->SetPosition(Float2(PosX, PosY));
}