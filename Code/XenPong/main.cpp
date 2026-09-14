//
// Created by Jake Rieger on 9/8/2026.
//

#include "BallComponent.hpp"
#include "OpponentComponent.hpp"
#include "PlayerComponent.hpp"

#include <Engine/Log.hpp>
#include <Engine/Window.hpp>
#include <Engine/AssetPreloader.hpp>
#include <Engine/Game.hpp>
#include <Engine/Scene.hpp>
#include <Engine/SceneSerializer.hpp>
#include <PAK/AssetMount.hpp>

using namespace Xen;

namespace {
    // Rules for asset mounting:
    // * For a release build, Pak file is required and the only option.
    // * For a debug build, both Pak and content dir will be mounted with content dir taking priority. Additional
    //   directories can be provided via `--content-dir` command args.
    PAK::AssetMountConfig GetMountConfig(const int argc, char* argv[]) {
        PAK::AssetMountConfig Config;

#ifdef NDEBUG
    #ifndef XEN_GAME_PAK_FILENAME
        #error "XEN_GAME_PAK_FILENAME must be defined for release builds"
    #endif

        Config.PakFiles = {XEN_GAME_PAK_FILENAME};

    #ifdef XEN_GAME_CONTENT_DIR
        Config.ContentDirs = {XEN_GAME_CONTENT_DIR};
    #endif
#else
    #ifndef XEN_GAME_CONTENT_DIR
        #error "XEN_GAME_CONTENT_DIR must be defined for debug builds"
    #endif

        Config.ContentDirs = {XEN_GAME_CONTENT_DIR};

    #ifdef XEN_GAME_PAK_FILENAME
        Config.PakFiles = {XEN_GAME_PAK_FILENAME};
    #endif

        // Append any content directories provided via arguments (--content-dir <path>)
        PAK::AppendContentDirsFromArgs(Config, argc, argv);
#endif

        return Config;
    }

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

        const auto ScenePath = std::filesystem::path(XEN_GAME_CONTENT_DIR) / "scenes" / "main.scene";
        SceneSerializer::SaveToFile(MainScene, ScenePath);

        _LogInfo("Scene saved: %s", ScenePath.string().c_str());
    }
}  // namespace

int main(const int argc, char* argv[]) {
    try {
        XenPong Game("XenPong", GetMountConfig(argc, argv));

        if (argc > 1) {
            if (std::string(argv[1]) == "--build-scene") {
                BuildScene(Game.GetContext());
                return 0;
            }
        }

        Game.Run();
    } catch (std::exception& Ex) {
        _LogCritical("%s", Ex.what());
        return -1;
    }
}