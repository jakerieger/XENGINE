//
// Created by Jake Rieger on 9/8/2026.
//

#include "PlayerComponent.hpp"

#include "Game.hpp"

#include <Xen/Scene.hpp>
#include <Xen/Actor.hpp>
#include <Xen/SpriteComponent.hpp>

#include <algorithm>

namespace Xen {
    void PlayerComponent::Reflect(IReflector& R) {}

    void PlayerComponent::BeginPlay() {
        if (const auto* Sprite = GetOwner()->GetComponent<SpriteComponent>()) {
            _HalfHeight = Sprite->GetWorldBounds().Height * 0.5f;
        }

        Reset();
    }

    void PlayerComponent::Tick(f32 DeltaTime) {}

    void PlayerComponent::FixedTick(const f32 FixedDelta) {
        const auto CurrentPos = GetOwner()->GetPosition();
        f32 PosY              = CurrentPos.y;

        if (GetGame()->GetInputManager().GetKeyDown(Input::KeyCode::Up)) { PosY += 8.f * FixedDelta; }
        if (GetGame()->GetInputManager().GetKeyDown(Input::KeyCode::Down)) { PosY -= 8.f * FixedDelta; }

        const auto* MainCamera = GetOwner()->GetScene()->GetMainCamera();
        const f32 BoundsY      = (MainCamera->GetViewportHeight() / MainCamera->GetPixelsPerUnit()) * 0.5f;

        PosY = std::clamp(PosY, -BoundsY + _HalfHeight, BoundsY - _HalfHeight);

        GetOwner()->SetPosition({CurrentPos.x, PosY});
    }

    void PlayerComponent::EndPlay() {}

    void PlayerComponent::Reset() {
        const auto MainCamera = GetOwner()->GetScene()->GetMainCamera();
        const auto BoundsX    = MainCamera->GetViewportWidth() / MainCamera->GetPixelsPerUnit();

        // Just inboard of the left edge, vertically centered (Y=0 is screen center,
        // not the top - GetViewBounds() spans -BoundsY/2..+BoundsY/2).
        const auto PosX = -(BoundsX / 2.1f);

        GetOwner()->SetPosition(Float2(PosX, 0.0f));
    }
}  // namespace Xen