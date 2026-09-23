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

        const ActorHandle TestMeshHandle = MainScene.Spawn("TestMesh");
        Actor* TestMeshActor             = MainScene.Get(TestMeshHandle);
        TestMeshActor->AddComponent<MeshComponent>(ASSET("meshes/teapot.glb"));

        auto* Material = TestMeshActor->AddComponent<PBRMaterialComponent>();
        Material->SetAlbedoMapAsset(ASSET("pbr/marble/albedo.png"));
        Material->SetNormalMapAsset(ASSET("pbr/marble/normal.png"));
        Material->SetRoughnessMapAsset(ASSET("pbr/marble/roughness.png"));
        Material->SetMetallic(0.1f);

        TestMeshActor->AddComponent<RotatingComponent>();
        TestMeshActor->SetPosition(Float3 {0.0f, 0.25f, 0.0f});
        TestMeshActor->SetScale(Float3 {0.5f, 0.5f, 0.5f});

        // A floor for the TestMeshy's shadow to land on.
        const ActorHandle GroundHandle = MainScene.Spawn("Ground");
        Actor* GroundActor             = MainScene.Get(GroundHandle);
        GroundActor->AddComponent<MeshComponent>(ASSET("meshes/plane.glb"));

        auto* GroundMaterial = GroundActor->AddComponent<PBRMaterialComponent>();
        GroundMaterial->SetAlbedoMapAsset(ASSET("pbr/checkered_tile/albedo.png"));
        GroundMaterial->SetNormalMapAsset(ASSET("pbr/checkered_tile/normal.png"));
        GroundMaterial->SetRoughnessMapAsset(ASSET("pbr/checkered_tile/roughness.png"));
        GroundMaterial->SetMetallic(0.01f);

        GroundActor->SetScale(Float3 {200.0f, 1.0f, 200.0f});

        const ActorHandle CameraHandle = MainScene.Spawn("MainCamera");
        Actor* CameraActor             = MainScene.Get(CameraHandle);
        auto* Camera                   = CameraActor->AddComponent<CameraComponent>();
        Camera->SetProjectionMode(ProjectionMode::Perspective);
        Camera->SetFieldOfView(60.0f);
        // In front of the origin along +Z, looking down -Z (this engine's
        // canonical forward, matching glTF's convention) at an unrotated
        // Transform - the TestMesh at the origin ends up straight ahead.
        CameraActor->SetPosition(Float3 {0.0f, 1.0f, 4.0f});

        const ActorHandle LightHandle = MainScene.Spawn("Light");
        Actor* LightActor             = MainScene.Get(LightHandle);
        auto* Light                   = LightActor->AddComponent<DirectionalLightComponent>();
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
        Quat LightRotationOut;
        XMStoreFloat4(&LightRotationOut, LightRotation);
        LightActor->SetRotation(LightRotationOut);

        const ActorHandle EnvironmentHandle = MainScene.Spawn("Environment");
        Actor* EnvironmentActor             = MainScene.Get(EnvironmentHandle);
        auto* Environment                   = EnvironmentActor->AddComponent<EnvironmentComponent>();
        Environment->SetMapAsset(ASSET("ibl/maps/sky_spring.hdr"));

        const ActorHandle PostFxHandle = MainScene.Spawn("PostProcess");
        Actor* PostFxActor             = MainScene.Get(PostFxHandle);
        auto* PostFx                   = PostFxActor->AddComponent<PostProcessComponent>();

        auto& FxSettings          = PostFx->GetSettings();
        FxSettings.BloomThreshold = 0.8f;
        FxSettings.BloomIntensity = 0.075f;
        FxSettings.BloomEnabled   = true;

        FxSettings.AutoExposureEnabled = true;

        const auto ScenePath = Generated::GameSettings().ContentDirs.front() / "scenes" / "main.xscene";
        SceneSerializer::SaveToFile(MainScene, ScenePath);

        LOG_INFO("Scene saved: %s", ScenePath.string().c_str());
    }
}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    Xen::FixContentWorkingDirectory();

    try {
        Xen::ProcessCommandLineArguments Arguments {};
        if (!Xen::GetProcessCommandLineArguments(Arguments)) {
            LOG_ERR("Failed to get command line arguments");
            return 1;
        }

        XenPBRDemo Game("Demo.PBR", BuildMountConfig(Generated::GameSettings(), Arguments.Argc, Arguments.Argv));

#ifndef NDEBUG
        BuildScene(Game.GetContext());
#endif

        Game.Run();
    } catch (...) { return 1; }

    return 0;
}
