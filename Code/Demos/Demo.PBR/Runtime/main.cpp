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
#include <Xen/EnvironmentComponent.hpp>
#include <Xen/PostProcessComponent.hpp>
#include <Xen/XenGameSettings.h>

#include <imgui.h>

namespace {
    using namespace Xen;

    class XenPBRDemo final : public Game {
        using Game::Game;

    protected:
        void OnStartup() override {}

        void OnUpdate(f32 DeltaTime) override {
            if (GetInputManager().GetKeyDown(Input::KeyCode::Escape)) { Quit(); }
        }

        // Example DebugUI usage: any ordinary ImGui:: call works here, since
        // Game already brackets this with DebugUI::BeginFrame/EndFrame. Must
        // still be guarded by IsInitialized() - a release build never
        // creates an ImGui context at all (see DebugUI.hpp's
        // XEN_WITH_DEBUG_UI), so calling ImGui:: unconditionally would crash.
        void OnRender() override {
            if (!GetDebugUI().IsInitialized()) return;

            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Frame Stats")) {
                const RHI::FrameStats& Stats = GetRenderDevice().GetLastFrameStats();
                const f32 Delta              = GetLastFrameDelta();

                ImGui::Text("Frame:  %llu", GetFrameCount());
                ImGui::Text("Delta:  %.2f ms (%.0f FPS)", Delta * 1000.0f, Delta > 0.0f ? 1.0f / Delta : 0.0f);
                ImGui::Separator();
                ImGui::Text("Draw calls:      %u", Stats.DrawCalls);
                ImGui::Text("Triangles:       %u", Stats.TriangleCount);
                ImGui::Text("Render passes:   %u", Stats.RenderPasses);
                ImGui::Text("Pipeline binds:  %u (%u redundant skipped)",
                            Stats.PipelineBinds,
                            Stats.RedundantBindsSkipped);
                ImGui::Text("Transient bytes: %u", Stats.TransientBytesUsed);

                ImGui::Separator();
                ImGui::Text("Asset Caches");
                const TextureCache& Textures = GetTextures();
                const MeshCache& Meshes      = GetMeshes();
                ImGui::Text("  Textures: %llu (%0.2f MB)",
                            CAST<u64>(Textures.GetResidentCount()),
                            ToMB(Textures.GetResidentBytes()));
                ImGui::Text("  Meshes:   %llu (%0.2f MB)",
                            CAST<u64>(Meshes.GetResidentCount()),
                            ToMB(Meshes.GetResidentBytes()));

                ImGui::Separator();
                ImGui::Text("Memory");
                // clang-format off
                const auto& [GpuAllocatedBytes,
                             GpuReservedBytes,
                             GpuUsageBytes,
                             GpuBudgetBytes,
                             ProcessRamBytes] = GetRenderDevice().GetMemoryStats();
                // clang-format on
                ImGui::Text("  GPU allocated: %0.2f MB", ToMB(GpuAllocatedBytes));
                ImGui::Text("  GPU reserved:  %0.2f MB", ToMB(GpuReservedBytes));
                ImGui::Text("  GPU usage:     %0.2f / %0.2f MB", ToMB(GpuUsageBytes), ToMB(GpuBudgetBytes));
                ImGui::Text("  Process RAM:   %0.2f MB", ToMB(ProcessRamBytes));
            }
            ImGui::End();
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

    // A single textured mesh over a ground plane, one shadow-casting
    // directional light and an environment map. See MeshRenderer and
    // Code/Shaders/PBR.hlsl.
    void BuildScene(const EngineContext& Ctx) {
        using namespace DirectX;

        Scene MainScene("Main");
        MainScene.SetContext(Ctx);

        const ActorHandle MonkeHandle = MainScene.Spawn("Monke");
        Actor* MonkeActor             = MainScene.Get(MonkeHandle);
        MonkeActor->AddComponent<MeshComponent>(ASSET("meshes/suzanne.glb"));
        auto* Material = MonkeActor->AddComponent<PBRMaterialComponent>();

        Material->SetAlbedoMapAsset(ASSET("pbr/sand/albedo.png"));
        Material->SetNormalMapAsset(ASSET("pbr/sand/normal.png"));
        Material->SetRoughnessMapAsset(ASSET("pbr/sand/roughness.png"));
        Material->SetAmbientOcclusionMapAsset(ASSET("pbr/sand/ao.png"));
        Material->SetMetallic(0.1f);

        MonkeActor->AddComponent<RotatingComponent>();
        MonkeActor->SetPosition(Float3 {0.0f, 1.0f, 0.0f});

        // A floor for the monkey's shadow to land on.
        const ActorHandle GroundHandle = MainScene.Spawn("Ground");
        Actor* GroundActor             = MainScene.Get(GroundHandle);
        GroundActor->AddComponent<MeshComponent>(ASSET("meshes/plane.gltf"));
        auto* GroundMaterial = GroundActor->AddComponent<PBRMaterialComponent>();
        GroundMaterial->SetAlbedo(Float3 {0.55f, 0.55f, 0.58f});
        GroundMaterial->SetRoughness(0.85f);
        GroundActor->SetScale(Float3 {200.0f, 1.0f, 200.0f});

        const ActorHandle CameraHandle = MainScene.Spawn("MainCamera");
        Actor* CameraActor             = MainScene.Get(CameraHandle);
        auto* Camera                   = CameraActor->AddComponent<CameraComponent>();
        Camera->SetProjectionMode(ProjectionMode::Perspective);
        Camera->SetFieldOfView(60.0f);
        // In front of the origin along +Z, looking down -Z (this engine's
        // canonical forward, matching glTF's convention) at an unrotated
        // Transform - the cube at the origin ends up straight ahead.
        CameraActor->SetPosition(Float3 {0.0f, 1.0f, 4.0f});

        const ActorHandle LightHandle = MainScene.Spawn("Light");
        Actor* LightActor             = MainScene.Get(LightHandle);
        // A bit stronger than a neutral 1.0 so the monkey's specular
        // highlights - not just the HDRI's own sun - cross the bloom
        // threshold too.
        LightActor->AddComponent<DirectionalLightComponent>()->SetIntensity(3.0f);
        // Pitched down (a negative pitch tilts the unrotated -Z forward
        // toward -Y) and yawed around so the light travels toward the camera:
        // the floor is lit, and the monkey's shadow falls in front of it
        // where the camera can see it.
        const XMVECTOR LightRotation =
          XMQuaternionRotationRollPitchYaw(XMConvertToRadians(-45.0f), XMConvertToRadians(150.0f), 0.0f);
        Quat LightRotationOut;
        XMStoreFloat4(&LightRotationOut, LightRotation);
        LightActor->SetRotation(LightRotationOut);

        // An equirectangular .hdr environment - the engine-shipped ones live
        // in Engine/Environment; Scripts/generate_test_hdri.py writes a small
        // synthetic sky+sun one for testing. Also drawn as the scene's
        // background (EnvironmentComponent::SetShowBackground to turn off).
        const ActorHandle EnvironmentHandle = MainScene.Spawn("Environment");
        Actor* EnvironmentActor             = MainScene.Get(EnvironmentHandle);
        auto* Environment                   = EnvironmentActor->AddComponent<EnvironmentComponent>();
        Environment->SetMapAsset(ASSET("ibl/maps/sky_day.hdr"));

        // Bloom on defaults would be nearly invisible here - the HDRI's sun
        // and the sky near it are the only things bright enough to cross the
        // threshold. A slightly stronger sun and a lower threshold make it
        // show without needing an artificially bright light.
        const ActorHandle PostFxHandle       = MainScene.Spawn("PostProcess");
        Actor* PostFxActor                   = MainScene.Get(PostFxHandle);
        auto* PostFx                         = PostFxActor->AddComponent<PostProcessComponent>();
        PostFx->GetSettings().BloomThreshold = 0.8f;
        PostFx->GetSettings().BloomIntensity = 0.12f;
        PostFx->GetSettings().BloomEnabled   = false;

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
        XenPBRDemo Game("Demo.PBR", BuildMountConfig(Generated::GameSettings(), __argc, __argv));

#ifndef NDEBUG
        BuildScene(Game.GetContext());
#endif

        Game.Run();
    } catch (...) { return 1; }

    return 0;
}
