//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Xen/Actor.hpp>
#include <Xen/Component.hpp>
#include <Xen/ComponentRegistry.hpp>
#include <../Modules/Common/XenCommon.hpp>
#include <random>

namespace Xen {
    class GameManagerComponent;

    REGISTER_COMPONENT(BallComponent)

    class BallComponent final : public IComponent {
    public:
        XEN_COMPONENT_TYPE(BallComponent);

        BallComponent();

        void Reflect(IReflector& R) override;
        void BeginPlay() override;
        void Tick(f32 DeltaTime) override;
        void FixedTick(f32 FixedDelta) override;
        void EndPlay() override;

        void Reset();

    private:
        void ApplySpeedGain();

        /// @brief Checks the ball's predicted position against one paddle's world bounds and,
        /// on overlap, mirrors Next.x off the paddle's near edge and flips _Velocity.x.
        bool TryBouncePaddle(const Actor* Paddle, Float2& Next);

        Float2 _Velocity {0.0f, 0.0f};
        f32 _BallSpeed {5.0f};
        Float2 _Bounds {0.0f, 0.0f};
        f32 _SpeedGain {1.0f};
        f32 _MaxBallSpeed {15.0f};
        f32 _InitialBallSpeed {_BallSpeed};

        // Cached at BeginPlay: the ball's own SpriteComponent has already resolved its texture
        // by then (it's added before this component on the actor), and the size doesn't change.
        Float2 _BallHalfSize {0.0f, 0.0f};

        Actor* _PlayerPaddle {nullptr};
        Actor* _OpponentPaddle {nullptr};
        GameManagerComponent* _GameManager {nullptr};

        std::mt19937 _Rng;
    };
}  // namespace Xen