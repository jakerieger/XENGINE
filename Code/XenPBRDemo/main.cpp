//
// Created by Jake Rieger on 9/17/2026.
//

#include "RotatingComponent.hpp"

#include <Xen/Window.hpp>
#include <Xen/AssetPreloader.hpp>
#include <Xen/Game.hpp>
#include <Xen/Scene.hpp>
#include <Xen/SceneSerializer.hpp>
#include <Xen/MeshComponent.hpp>
#include <Xen/PBRMaterialComponent.hpp>
#include <Xen/DirectionalLightComponent.hpp>
#include <Xen/XenGameSettings.h>

namespace {
    using namespace Xen;

    class XenPBRDemo final : public Game {
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

    // First milestone for the 3D/PBR groundwork: a single mesh, one
    // directional light, no textures/shadows/IBL yet. See MeshRenderer and
    // Content/shaders/pbr.hlsl.
    void BuildScene(const EngineContext& Ctx) {
        using namespace DirectX;

        Scene MainScene("Main");
        MainScene.SetContext(Ctx);

        const ActorHandle CubeHandle = MainScene.Spawn("Cube");
        Actor* CubeActor             = MainScene.Get(CubeHandle);
        CubeActor->AddComponent<MeshComponent>(ASSET("meshes/cube.xmesh"));
        auto* Material = CubeActor->AddComponent<PBRMaterialComponent>();
        Material->SetAlbedo({0.7f, 0.15f, 0.15f});
        Material->SetMetallic(0.2f);
        Material->SetRoughness(0.35f);
        CubeActor->AddComponent<RotatingComponent>();

        const ActorHandle CameraHandle = MainScene.Spawn("MainCamera");
        Actor* CameraActor             = MainScene.Get(CameraHandle);
        auto* Camera                   = CameraActor->AddComponent<CameraComponent>();
        Camera->SetProjectionMode(ProjectionMode::Perspective);
        Camera->SetFieldOfView(60.0f);
        // Behind the origin along -Z, looking down +Z (this engine's
        // canonical forward) at an unrotated Transform - the cube at the
        // origin ends up straight ahead.
        CameraActor->SetPosition(Float3 {0.0f, 1.0f, -4.0f});

        const ActorHandle LightHandle = MainScene.Spawn("Light");
        Actor* LightActor             = MainScene.Get(LightHandle);
        LightActor->AddComponent<DirectionalLightComponent>();
        // Angled so the cube's faces shade differently instead of a single
        // flat-lit silhouette - pitched down and yawed off-axis.
        const XMVECTOR LightRotation =
          XMQuaternionRotationRollPitchYaw(XMConvertToRadians(45.0f), XMConvertToRadians(-30.0f), 0.0f);
        Quat LightRotationOut;
        XMStoreFloat4(&LightRotationOut, LightRotation);
        LightActor->SetRotation(LightRotationOut);

        const auto ScenePath = Generated::GameSettings().ContentDirs[0] / "scenes" / "main.xscene";
        SceneSerializer::SaveToFile(MainScene, ScenePath);

        LOG_INFO("Scene saved: %s", ScenePath.string().c_str());
    }
}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    try {
        XenPBRDemo Game("XenPBRDemo", BuildMountConfig(Generated::GameSettings(), __argc, __argv));

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
