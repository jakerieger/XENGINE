//
// Created by Jake Rieger on 9/8/2026.
//

#include "Source/BallComponent.hpp"
#include "Source/GameManagerComponent.hpp"
#include "Source/OpponentComponent.hpp"
#include "Source/PlayerComponent.hpp"

#include <Xen/Window.hpp>
#include <Xen/AssetPreloader.hpp>
#include <Xen/Game.hpp>
#include <Xen/Scene.hpp>
#include <Xen/SceneSerializer.hpp>
#include <Xen/XenGameSettings.h>

namespace {
    using namespace Xen;

    class XenPong final : public Game {
        using Game::Game;

    protected:
        void OnStartup() override {}

        void OnUpdate(f32 DeltaTime) override {
            if (GetInputManager().GetKeyDown(Input::KeyCode::Escape)) { Quit(); }
        }

        void OnSceneLoaded(Scene& S) override {
            LOG_INFO("Loaded scene: %s", S.GetName().c_str());
            LOG_INFO("Actors in scene: %llu", S.GetActorCount());
            LOG_INFO("Actors:");

            S.ForEachActor([](const Actor& A) {
                LOG_INFO("  - %s (%llu component(s))", A.GetName().c_str(), A.GetComponentCount());
            });
        }

        void OnSceneUnloading(Scene& S) override { LOG_INFO("Scene unloading: %s", S.GetName().c_str()); }
    };

    // TODO: Move this logic out of the game executable and into some kind of separate game library so it can be called
    // from an independent tool. Eventually this will be integrated into an editor of some kind.
    void BuildScene(const EngineContext& Ctx) {
        Scene MainScene("Main");
        MainScene.SetContext(Ctx);

        const ActorHandle BallHandle = MainScene.Spawn("Ball");
        Actor* BallActor             = MainScene.Get(BallHandle);
        BallActor->AddComponent<SpriteComponent>(ASSET("sprites/ball.png"));
        BallActor->AddComponent<BallComponent>();

        const ActorHandle PlayerHandle = MainScene.Spawn("Player");
        Actor* PlayerActor             = MainScene.Get(PlayerHandle);
        PlayerActor->AddComponent<SpriteComponent>(ASSET("sprites/paddle_player.png"));
        PlayerActor->AddComponent<PlayerComponent>();

        const ActorHandle OpponentHandle = MainScene.Spawn("Opponent");
        Actor* OpponentActor             = MainScene.Get(OpponentHandle);
        OpponentActor->AddComponent<SpriteComponent>(ASSET("sprites/paddle_opponent.png"));
        OpponentActor->AddComponent<OpponentComponent>();

        const ActorHandle CameraHandle = MainScene.Spawn("MainCamera");
        Actor* CameraActor             = MainScene.Get(CameraHandle);
        CameraActor->AddComponent<CameraComponent>();

        const ActorHandle GameManagerHandle = MainScene.Spawn("GameManager");
        Actor* GameManagerActor             = MainScene.Get(GameManagerHandle);
        GameManagerActor->AddComponent<GameManagerComponent>();

        const auto ScenePath = Generated::GameSettings().ContentDirs[0] / "scenes" / "main.xscene";
        SceneSerializer::SaveToFile(MainScene, ScenePath);

        LOG_INFO("Scene saved: %s", ScenePath.string().c_str());
    }
}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    // Must run before BuildMountConfig: the .exe lives in Bin64/ now, one
    // level below Config/Data1.xpak/Engine/, and every relative path in the
    // engine is still written as if the .exe were where it used to be. See
    // FixContentWorkingDirectory's own comment for the full explanation.
    Xen::FixContentWorkingDirectory();

    try {
        XenPong Game("XenPong", BuildMountConfig(Generated::GameSettings(), __argc, __argv));

#ifndef NDEBUG
        BuildScene(Game.GetContext());
#endif

        Game.Run();
    } catch (const EngineException& Ex) {
        LOG_CRIT("%s", Ex.what());
        return 1;
    }

    return 0;
}
