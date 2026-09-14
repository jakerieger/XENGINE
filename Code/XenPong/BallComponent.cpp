//
// Created by Jake Rieger on 9/8/2026.
//

#include "BallComponent.hpp"
#include "Xen/Scene.hpp"

using namespace Xen;

BallComponent::BallComponent() {}

void BallComponent::Reflect(IReflector& R) {
    R.Property("BallSpeed", _BallSpeed, {.Tooltip = "World units per second.", .Category = "Ball"});
    R.Property("SpeedGain", _SpeedGain, {.Tooltip = "Speed multiplier per bounce.", .Category = "Ball"});
}

void BallComponent::BeginPlay() {
    std::random_device Rd;
    _Rng.seed(Rd());
    Reset();
}

void BallComponent::Tick(f32) {
    // For now, just update bounds every frame. In the future, doing this only when a resize is triggered would be
    // preferential.
    if (const auto MainCamera = GetOwner()->GetScene()->GetMainCamera()) {
        _Bounds.x = MainCamera->GetViewBounds().Width / 2;
        _Bounds.y = MainCamera->GetViewBounds().Height / 2;
    }
}

void BallComponent::FixedTick(const f32 FixedDelta) {
    const glm::vec2 Current = GetOwner()->GetPosition();
    glm::vec2 Next          = Current + _Velocity * _BallSpeed * FixedDelta;

    if (Next.y > _Bounds.y) {
        Next.y      = 2.0f * _Bounds.y - Next.y;
        _Velocity.y = -_Velocity.y;
        _BallSpeed *= _SpeedGain;
    } else if (Next.y < -_Bounds.y) {
        Next.y      = -2.0f * _Bounds.y - Next.y;
        _Velocity.y = -_Velocity.y;
        _BallSpeed *= _SpeedGain;
    }

    if (Next.x > _Bounds.x) {
        Next.x      = 2.0f * _Bounds.x - Next.x;
        _Velocity.x = -_Velocity.x;
    } else if (Next.x < -_Bounds.x) {
        Next.x      = -2.0f * _Bounds.x - Next.x;
        _Velocity.x = -_Velocity.x;
    }

    GetOwner()->SetPosition(Next);
}

void BallComponent::EndPlay() {}

void BallComponent::Reset() {
    GetOwner()->SetPosition({0.0f, 0.0f});

    std::uniform_real_distribution Angle(0.0f, glm::two_pi<f32>());

    f32 Theta = 0.0f;
    for (int Attempt = 0; Attempt < 16; ++Attempt) {
        Theta = Angle(_Rng);
        if (std::abs(std::cos(Theta)) > 0.35f) break;
    }

    _Velocity = {std::cos(Theta), std::sin(Theta)};
}