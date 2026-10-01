//
// Created by Jake Rieger on 9/23/2026.
//

#pragma once

#include <Xen/Game.hpp>

class Sandbox final : public Xen::Game {
    using Game::Game;

protected:
    void OnStartup() override {}
    void OnUpdate(Xen::f32 DeltaTime) override;

    // Example DebugUI usage: any ordinary ImGui:: call works here, since
    // Game already brackets this with DebugUI::BeginFrame/EndFrame. Must
    // still be guarded by IsInitialized() - a release build never
    // creates an ImGui context at all (see DebugUI.hpp's
    // XEN_WITH_DEBUG_UI), so calling ImGui:: unconditionally would crash.
    void OnRender() override;

    void OnSceneLoaded(Xen::Scene& S) override;
    void OnSceneUnloading(Xen::Scene& S) override;
};
