//
// Created by Jake Rieger on 9/16/2026.
//

#pragma once

#include <Xen/Component.hpp>
#include <Xen/ComponentRegistry.hpp>

namespace Xen {
    class PlayerComponent;
    class OpponentComponent;
    class BallComponent;

    enum class GameState : u8 {
        Ongoing,
        PlayerWon,
        OpponentWon,
    };

    REGISTER_COMPONENT(GameManagerComponent);
    class GameManagerComponent final : public IComponent {
        friend class BallComponent;

    public:
        XEN_COMPONENT_TYPE(GameManagerComponent);
        GameManagerComponent() = default;

        void Reflect(IReflector& R) override;
        void BeginPlay() override;

        void ResetGame();

        GameState GetGameState() const;

    private:
        static constexpr i16 PLAYER_ID {1};
        static constexpr i16 OPPONENT_ID {2};

        u32 _PlayerScore {0};
        u32 _OpponentScore {0};
        u32 _ScoreToWin {5};

        PlayerComponent* _Player {nullptr};
        OpponentComponent* _Opponent {nullptr};
        BallComponent* _Ball {nullptr};

        void AddPlayerScore();
        void AddOpponentScore();
    };
}  // namespace Xen
