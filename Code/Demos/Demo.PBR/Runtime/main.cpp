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
#include <Xen/XenGameSettings.h>

#include <imgui.h>

namespace {
    using namespace Xen;

    f32 ToMB(const u64 Bytes) {
        return CAST<f32>(Bytes) / (1024.0f * 1024.0f);
    }

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
                ImGui::Text("  Textures: %llu (%.2f MB)",
                            CAST<u64>(Textures.GetResidentCount()),
                            ToMB(Textures.GetResidentBytes()));
                ImGui::Text("  Meshes:   %llu (%.2f MB)",
                            CAST<u64>(Meshes.GetResidentCount()),
                            ToMB(Meshes.GetResidentBytes()));

                ImGui::Separator();
                ImGui::Text("Memory");
                const RHI::MemoryStats& Mem = GetRenderDevice().GetMemoryStats();
                ImGui::Text("  GPU allocated: %.2f MB", ToMB(Mem.GpuAllocatedBytes));
                ImGui::Text("  GPU reserved:  %.2f MB", ToMB(Mem.GpuReservedBytes));
                ImGui::Text("  GPU usage:     %.2f / %.2f MB", ToMB(Mem.GpuUsageBytes), ToMB(Mem.GpuBudgetBytes));
                ImGui::Text("  Process RAM:   %.2f MB", ToMB(Mem.ProcessRamBytes));
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

    // A single textured mesh, one directional light and an environment map
    // (no shadows yet). See MeshRenderer and Code/Shaders/PBR.hlsl.
    void BuildScene(const EngineContext& Ctx) {
        using namespace DirectX;

        Scene MainScene("Main");
        MainScene.SetContext(Ctx);

        const ActorHandle MonkeHandle = MainScene.Spawn("Monke");
        Actor* MonkeActor             = MainScene.Get(MonkeHandle);
        MonkeActor->AddComponent<MeshComponent>(ASSET("meshes/suzanne.glb"));
        auto* Material = MonkeActor->AddComponent<PBRMaterialComponent>();

        Material->SetAlbedoMapAsset(ASSET("textures/pbr_gold_worn_albedo.png"));
        Material->SetMetallicRoughnessMapAsset(ASSET("textures/pbr_gold_worn_mr.png"));
        Material->SetNormalMapAsset(ASSET("textures/pbr_gold_worn_normal.png"));
        Material->SetMetallic(1.0f);

        MonkeActor->AddComponent<RotatingComponent>();
        MonkeActor->SetPosition(Float3 {0.0f, 1.0f, 0.0f});

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
        LightActor->AddComponent<DirectionalLightComponent>();
        // Angled so the cube's faces shade differently instead of a single
        // flat-lit silhouette - pitched down and yawed off-axis.
        const XMVECTOR LightRotation =
          XMQuaternionRotationRollPitchYaw(XMConvertToRadians(45.0f), XMConvertToRadians(-30.0f), 0.0f);
        Quat LightRotationOut;
        XMStoreFloat4(&LightRotationOut, LightRotation);
        LightActor->SetRotation(LightRotationOut);

        // Synthetic sky+sun test map - regenerate with
        // Scripts/generate_test_hdri.py. Swap in a real equirectangular .hdr
        // by changing this asset path.
        const ActorHandle EnvironmentHandle = MainScene.Spawn("Environment");
        Actor* EnvironmentActor             = MainScene.Get(EnvironmentHandle);
        auto* Environment                   = EnvironmentActor->AddComponent<EnvironmentComponent>();
        Environment->SetMapAsset(ASSET("xen.hdr.spring.hdr"));

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
    } catch (const EngineException& Ex) {
        LOG_CRIT("%s", Ex.what());
        return 1;
    }

    return 0;
}
