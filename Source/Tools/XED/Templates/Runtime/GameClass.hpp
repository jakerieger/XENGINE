//
// Created by ${USER} on ${DATE}
//

#pragma once

#include <Xen/Game.hpp>

class ${GAME_CLASS} final : public Xen::Game {
    using Game::Game;

protected:
    void OnStartup() override;
    void OnUpdate(Xen::f32 DeltaTime) override;
    void OnRender() override;
};