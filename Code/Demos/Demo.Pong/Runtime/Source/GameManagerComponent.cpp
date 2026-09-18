//
// Created by Jake Rieger on 9/16/2026.
//

#include "GameManagerComponent.hpp"
#include "PlayerComponent.hpp"
#include "OpponentComponent.hpp"
#include "BallComponent.hpp"
#include <Xen/Scene.hpp>

namespace Xen {
    void GameManagerComponent::Reflect(IReflector& R) {
        R.Property("ScoreToWin",
                   _ScoreToWin,
                   {.ToolTip = "Score needed before a player wins the match", .Category = "GameManager"});
    }

    void GameManagerComponent::BeginPlay() {
        LOG_INFO("Starting match.");

        if (Scene* S = GetScene()) {
            const auto Players   = S->FindActorsWith<PlayerComponent>();
            const auto Opponents = S->FindActorsWith<OpponentComponent>();
            const auto Balls     = S->FindActorsWith<BallComponent>();

            if (Players.front()) { _Player = Players.front()->GetComponent<PlayerComponent>(); }
            if (Opponents.front()) { _Opponent = Opponents.front()->GetComponent<OpponentComponent>(); }
            if (Balls.front()) { _Ball = Balls.front()->GetComponent<BallComponent>(); }
        }
    }

    void GameManagerComponent::ResetGame() {
        _PlayerScore   = 0;
        _OpponentScore = 0;

        if (_Player) _Player->Reset();
        if (_Opponent) _Opponent->Reset();
        if (_Ball) _Ball->Reset();
    }

    GameState GameManagerComponent::GetGameState() const {
        if (!(_PlayerScore >= _ScoreToWin || _OpponentScore >= _ScoreToWin)) return GameState::Ongoing;
        return _PlayerScore > _OpponentScore ? GameState::PlayerWon : GameState::OpponentWon;
    }

    void GameManagerComponent::AddPlayerScore() {
        _PlayerScore++;
        LOG_INFO("PlayerScore: %d", _PlayerScore);
    }

    void GameManagerComponent::AddOpponentScore() {
        _OpponentScore++;
        LOG_INFO("OpponentScore: %d", _OpponentScore);
    }
}  // namespace Xen