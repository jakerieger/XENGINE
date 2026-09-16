//
// Created by Jake Rieger on 9/8/2026.
//

#include "BallComponent.hpp"
#include "OpponentComponent.hpp"
#include "PlayerComponent.hpp"

#include <Xen/Scene.hpp>
#include <Xen/SpriteComponent.hpp>

#include <algorithm>

using namespace Xen;

BallComponent::BallComponent() {}

void BallComponent::Reflect(IReflector& R) {
    R.Property("BallSpeed", _BallSpeed, {.Tooltip = "World units per second.", .Category = "Ball"});
    R.Property("SpeedGain", _SpeedGain, {.Tooltip = "Speed multiplier per bounce.", .Category = "Ball"});
    R.Property("MaxBallSpeed",
               _MaxBallSpeed,
               {.Tooltip = "Speed gain stops once this is reached.", .Category = "Ball"});
}

void BallComponent::BeginPlay() {
    std::random_device Rd;
    _Rng.seed(Rd());

    if (const auto* Sprite = GetOwner()->GetComponent<SpriteComponent>()) {
        const Rect Bounds = Sprite->GetWorldBounds();
        _BallHalfSize     = {Bounds.Width * 0.5f, Bounds.Height * 0.5f};
    }

    if (Scene* S = GetOwner()->GetScene()) {
        const std::vector<Actor*> Players   = S->FindActorsWith<PlayerComponent>();
        const std::vector<Actor*> Opponents = S->FindActorsWith<OpponentComponent>();
        _PlayerPaddle                       = Players.empty() ? nullptr : Players.front();
        _OpponentPaddle                     = Opponents.empty() ? nullptr : Opponents.front();
    }

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
    const Float2 Current = GetOwner()->GetPosition();
    Float2 Next           = Current + _Velocity * _BallSpeed * FixedDelta;

    if (Next.y > _Bounds.y) {
        Next.y      = 2.0f * _Bounds.y - Next.y;
        _Velocity.y = -_Velocity.y;
        ApplySpeedGain();
    } else if (Next.y < -_Bounds.y) {
        Next.y      = -2.0f * _Bounds.y - Next.y;
        _Velocity.y = -_Velocity.y;
        ApplySpeedGain();
    }

    if (TryBouncePaddle(_PlayerPaddle, Next) || TryBouncePaddle(_OpponentPaddle, Next)) {
        ApplySpeedGain();
    } else if (Next.x > _Bounds.x) {
        Next.x      = 2.0f * _Bounds.x - Next.x;
        _Velocity.x = -_Velocity.x;
        ApplySpeedGain();
    } else if (Next.x < -_Bounds.x) {
        Next.x      = -2.0f * _Bounds.x - Next.x;
        _Velocity.x = -_Velocity.x;
        ApplySpeedGain();
    }

    GetOwner()->SetPosition(Next);
}

void BallComponent::ApplySpeedGain() {
    _BallSpeed = std::min(_BallSpeed * _SpeedGain, _MaxBallSpeed);
}

bool BallComponent::TryBouncePaddle(const Actor* Paddle, Float2& Next) {
    if (!Paddle) return false;

    const auto* PaddleSprite = Paddle->GetComponent<SpriteComponent>();
    if (!PaddleSprite) return false;

    const Rect PaddleBounds = PaddleSprite->GetWorldBounds();
    const Rect BallBounds {Next.x - _BallHalfSize.x,
                           Next.y - _BallHalfSize.y,
                           _BallHalfSize.x * 2.0f,
                           _BallHalfSize.y * 2.0f};

    if (!BallBounds.Intersects(PaddleBounds)) return false;

    // Mirror the ball's center off whichever edge it's approaching from, based on travel
    // direction rather than which named paddle this is - works regardless of which side
    // the paddle sits on.
    const f32 EdgeX =
      _Velocity.x > 0.0f ? PaddleBounds.Left() - _BallHalfSize.x : PaddleBounds.Right() + _BallHalfSize.x;
    Next.x      = 2.0f * EdgeX - Next.x;
    _Velocity.x = -_Velocity.x;

    return true;
}

void BallComponent::EndPlay() {}

void BallComponent::Reset() {
    GetOwner()->SetPosition({0.0f, 0.0f});

    std::uniform_real_distribution Angle(0.0f, TWO_PI);

    f32 Theta = 0.0f;
    for (int Attempt = 0; Attempt < 16; ++Attempt) {
        Theta = Angle(_Rng);
        if (std::abs(std::cos(Theta)) > 0.35f) break;
    }

    _Velocity = {std::cos(Theta), std::sin(Theta)};
}