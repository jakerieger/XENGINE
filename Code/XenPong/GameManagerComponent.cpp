//
// Created by Jake Rieger on 9/16/2026.
//

#include "GameManagerComponent.hpp"

namespace Xen {
    void GameManagerComponent::Reflect(IReflector& R) {
        R.Property("ScoreToWin",
                   _ScoreToWin,
                   {.ToolTip = "Score needed before a player wins the match", .Category = "GameManager"});
    }
}  // namespace Xen