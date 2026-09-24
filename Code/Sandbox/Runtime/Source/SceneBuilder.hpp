//
// Created by Jake Rieger on 9/23/2026.
//

#pragma once

#include <Common/Math.hpp>

#include <Xen/XenGameSettings.h>
#include <Xen/EngineContext.hpp>
#include <Xen/Window.hpp>
#include <Xen/AssetPreloader.hpp>
#include <Xen/Game.hpp>
#include <Xen/Scene.hpp>
#include <Xen/SceneSerializer.hpp>

#include <Xen/Components/MeshComponent.hpp>
#include <Xen/Components/PBRMaterialComponent.hpp>
#include <Xen/Components/DirectionalLightComponent.hpp>
#include <Xen/Components/EnvironmentComponent.hpp>
#include <Xen/Components/PostProcessComponent.hpp>

#include "RotatingComponent.hpp"

namespace SceneBuilder {
    inline void Build(const Xen::EngineContext& Ctx) {
        using namespace DirectX;

        Xen::Scene MainScene("Main");
        MainScene.SetContext(Ctx);

        const Xen::ActorHandle TestMeshHandle = MainScene.Spawn("TestMesh");
        Xen::Actor* TestMeshActor             = MainScene.Get(TestMeshHandle);
        TestMeshActor->AddComponent<Xen::MeshComponent>(Xen::ASSET("meshes/teapot.glb"));

        auto* Material = TestMeshActor->AddComponent<Xen::PBRMaterialComponent>();
        Material->SetAlbedoMapAsset(Xen::ASSET("textures/marble/albedo.png"));
        Material->SetNormalMapAsset(Xen::ASSET("textures/marble/normal.png"));
        Material->SetRoughnessMapAsset(Xen::ASSET("textures/marble/roughness.png"));
        Material->SetMetallic(0.1f);

        TestMeshActor->AddComponent<Xen::RotatingComponent>();
        TestMeshActor->SetPosition(Xen::Float3 {0.0f, 0.25f, 0.0f});
        TestMeshActor->SetScale(Xen::Float3 {0.5f, 0.5f, 0.5f});

        // A floor for the TestMeshy's shadow to land on.
        const Xen::ActorHandle GroundHandle = MainScene.Spawn("Ground");
        Xen::Actor* GroundActor             = MainScene.Get(GroundHandle);
        GroundActor->AddComponent<Xen::MeshComponent>(Xen::ASSET("meshes/plane.glb"));

        auto* GroundMaterial = GroundActor->AddComponent<Xen::PBRMaterialComponent>();
        GroundMaterial->SetAlbedoMapAsset(Xen::ASSET("textures/checkered_tile/albedo.png"));
        GroundMaterial->SetNormalMapAsset(Xen::ASSET("textures/checkered_tile/normal.png"));
        GroundMaterial->SetRoughnessMapAsset(Xen::ASSET("textures/checkered_tile/roughness.png"));
        GroundMaterial->SetMetallic(0.01f);

        GroundActor->SetScale(Xen::Float3 {200.0f, 1.0f, 200.0f});

        const Xen::ActorHandle CameraHandle = MainScene.Spawn("MainCamera");
        Xen::Actor* CameraActor             = MainScene.Get(CameraHandle);
        auto* Camera                        = CameraActor->AddComponent<Xen::CameraComponent>();
        Camera->SetProjectionMode(Xen::ProjectionMode::Perspective);
        Camera->SetFieldOfView(60.0f);
        // In front of the origin along +Z, looking down -Z (this engine's
        // canonical forward, matching glTF's convention) at an unrotated
        // Transform - the TestMesh at the origin ends up straight ahead.
        CameraActor->SetPosition(Xen::Float3 {0.0f, 1.0f, 4.0f});

        const Xen::ActorHandle LightHandle = MainScene.Spawn("Light");
        Xen::Actor* LightActor             = MainScene.Get(LightHandle);
        auto* Light                        = LightActor->AddComponent<Xen::DirectionalLightComponent>();
        Light->SetIntensity(1.0f);
        // The single-cascade shadow map is fitted to the camera's whole view
        // frustum out to this distance, not to the TestMeshy specifically - the
        // engine default (40) is sized for a typical outdoor scene, but this
        // demo's camera sits only ~4 units from a ~2-unit-wide subject, so
        // most of that range bought nothing but coarser texels where it
        // actually mattered: 2048 texels over the ~94-unit diameter that
        // covers left the TestMeshy barely 40-50 texels wide, blocky enough to
        // read as jagged, shadow-map-texel-aligned edges that visibly didn't
        // track the mesh's own smooth rotation. Tightened to roughly triple
        // the effective resolution where this scene actually needs it.
        Light->SetShadowDistance(15.0f);
        // Pitched down (a negative pitch tilts the unrotated -Z forward
        // toward -Y) and yawed around so the light travels toward the camera:
        // the floor is lit, and the TestMeshy's shadow falls in front of it
        // where the camera can see it.
        const XMVECTOR LightRotation =
          XMQuaternionRotationRollPitchYaw(XMConvertToRadians(-45.0f), XMConvertToRadians(150.0f), 0.0f);
        Xen::Quat LightRotationOut;
        XMStoreFloat4(&LightRotationOut, LightRotation);
        LightActor->SetRotation(LightRotationOut);

        const Xen::ActorHandle EnvironmentHandle = MainScene.Spawn("Environment");
        Xen::Actor* EnvironmentActor             = MainScene.Get(EnvironmentHandle);
        auto* Environment                        = EnvironmentActor->AddComponent<Xen::EnvironmentComponent>();
        Environment->SetMapAsset(Xen::ASSET("ibl/maps/sky_spring.hdr"));

        const Xen::ActorHandle PostFxHandle = MainScene.Spawn("PostProcess");
        Xen::Actor* PostFxActor             = MainScene.Get(PostFxHandle);
        auto* PostFx                        = PostFxActor->AddComponent<Xen::PostProcessComponent>();

        auto& FxSettings          = PostFx->GetSettings();
        FxSettings.BloomThreshold = 0.8f;
        FxSettings.BloomIntensity = 0.075f;
        FxSettings.BloomEnabled   = true;

        FxSettings.AutoExposureEnabled = true;

        const auto ScenePath = Xen::Generated::GameSettings().ContentDirs.front() / "scenes" / "main.xscene";
        Xen::SceneSerializer::SaveToFile(MainScene, ScenePath);

        LOG_INFO("Scene saved: %s", ScenePath.string().c_str());
    }
}  // namespace SceneBuilder