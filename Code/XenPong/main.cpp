//
// Created by Jake Rieger on 9/8/2026.
//

#include "BallComponent.hpp"
#include "OpponentComponent.hpp"
#include "PlayerComponent.hpp"

#include <Xen/Log.hpp>
#include <Xen/Window.hpp>
#include <Xen/AssetPreloader.hpp>
#include <Xen/Game.hpp>
#include <Xen/Scene.hpp>
#include <Xen/SceneSerializer.hpp>
#include <Xen/XenGameSettings.h>

using namespace Xen;

namespace {
    class XenPong final : public Game {
        using Game::Game;

    protected:
        void OnStartup() override {}

        void OnUpdate(f32 DeltaTime) override {}

        void OnSceneLoaded(Scene& S) override {
            _LogInfo("Loaded scene: %s", S.GetName().c_str());
            _LogInfo("Actors in scene: %llu", S.GetActorCount());
            _LogInfo("Actors:");

            S.ForEachActor([](const Actor& A) {
                _LogInfo("  - %s (%llu component(s))", A.GetName().c_str(), A.GetComponentCount());
            });
        }

        void OnSceneUnloading(Scene& S) override { _LogInfo("Scene unloading: %s", S.GetName().c_str()); }
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

        const auto ScenePath = Generated::GameSettings().ContentDirs[0] / "scenes" / "main.scene";
        SceneSerializer::SaveToFile(MainScene, ScenePath);

        _LogInfo("Scene saved: %s", ScenePath.string().c_str());
    }
}  // namespace

XEN_GAME(XenPong, "XenPong")
