//
// Created by Jake Rieger on 9/27/2026.
//

#include "Editor.hpp"

#include <Xen/XenGameSettings.h>
#include <Xen/Scene.hpp>
#include <Xen/SceneSerializer.hpp>
#include <Xen/Components/MeshComponent.hpp>
#include <Xen/Components/PBRMaterialComponent.hpp>
#include <Xen/Components/CameraComponent.hpp>
#include <Xen/Components/DirectionalLightComponent.hpp>
#include <Xen/Components/EnvironmentComponent.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <chrono>

#pragma region Embedded Resources
#include "Resource/InterRegular.h"
#pragma endregion

namespace Xen {
    Editor::Editor() {
        _EditorWindow = std::make_unique<Window>("XED", EngineConfig::WindowMode::Windowed, 1600, 900);
        if (!_EditorWindow) { THROW_ENGINE_EXCEPTION(EditorException, "failed to create editor window"); }

        _Device = RHI::CreateRenderDevice(RHI::Backend::D3D12);
        if (!_Device) { THROW_ENGINE_EXCEPTION(EditorException, "no render device for the requested backend"); }

        RHI::DeviceDescriptor Descriptor {};
        Descriptor.NativeWindowHandle = _EditorWindow->GetHandle();
#ifndef NDEBUG
        Descriptor.EnableValidation   = true;
        Descriptor.EnableDebugMarkers = true;
#endif
        if (!_Device->Initialize(Descriptor)) {
            THROW_ENGINE_EXCEPTION(EditorException, "failed to initialize render device");
        }
        _Device->SetSwapChainSize(_EditorWindow->GetWidth(), _EditorWindow->GetHeight());

        if (!_DebugUI.Initialize(*_Device, *_EditorWindow)) {
            THROW_ENGINE_EXCEPTION(EditorException, "failed to initialize DebugUI");
        }
        _EditorWindow->SetDebugUI(&_DebugUI);

        if (!_DebugUI.LoadFont("inter", INTERREGULAR_TTF_BYTES, INTERREGULAR_TTF_SIZE, 16.0f)) {
            THROW_ENGINE_EXCEPTION(EditorException, "failed to load font");
        }

        // Sized once here to something reasonable; the "Scene" panel's own
        // content-region size takes over from the first real layout pass
        // (see DrawDockspaceAndPanels).
        _SceneViewportWidth  = 1280;
        _SceneViewportHeight = 720;

        // Reuses Sandbox's own content (see CMakeLists.txt) - the real
        // project/content system is later, separate work.
        const auto MountConfig = BuildMountConfig(Xen::Generated::GameSettings(), 0, nullptr);
        _EmbeddedGame = std::make_unique<Game>(*_Device, MountConfig, _SceneViewportWidth, _SceneViewportHeight);

        const std::filesystem::path ScenePath = BuildTestScene(_EmbeddedGame->GetContext());
        _EmbeddedGame->LoadSceneFromFile(ScenePath);
        _EmbeddedGame->StartEmbedded();
    }

    std::filesystem::path Editor::BuildTestScene(const EngineContext& Ctx) const {
        Scene TestScene("XED Test Scene");
        TestScene.SetContext(Ctx);

        const ActorHandle MeshHandle = TestScene.Spawn("TestMesh");
        Actor* MeshActor             = TestScene.Get(MeshHandle);
        MeshActor->AddComponent<MeshComponent>(ASSET("meshes/teapot.glb"));
        auto* Material = MeshActor->AddComponent<PBRMaterialComponent>();
        Material->SetAlbedoMapAsset(ASSET("textures/marble/albedo.png"));
        Material->SetNormalMapAsset(ASSET("textures/marble/normal.png"));
        Material->SetRoughnessMapAsset(ASSET("textures/marble/roughness.png"));
        Material->SetMetallic(0.1f);
        MeshActor->SetPosition(Float3 {0.0f, 0.25f, 0.0f});
        MeshActor->SetScale(Float3 {0.5f, 0.5f, 0.5f});

        const ActorHandle GroundHandle = TestScene.Spawn("Ground");
        Actor* GroundActor             = TestScene.Get(GroundHandle);
        GroundActor->AddComponent<MeshComponent>(ASSET("meshes/plane.glb"));
        auto* GroundMaterial = GroundActor->AddComponent<PBRMaterialComponent>();
        GroundMaterial->SetAlbedoMapAsset(ASSET("textures/checkered_tile/albedo.png"));
        GroundMaterial->SetNormalMapAsset(ASSET("textures/checkered_tile/normal.png"));
        GroundMaterial->SetRoughnessMapAsset(ASSET("textures/checkered_tile/roughness.png"));
        GroundMaterial->SetMetallic(0.01f);
        GroundActor->SetScale(Float3 {200.0f, 1.0f, 200.0f});

        const ActorHandle CameraHandle = TestScene.Spawn("MainCamera");
        Actor* CameraActor             = TestScene.Get(CameraHandle);
        auto* Camera                   = CameraActor->AddComponent<CameraComponent>();
        Camera->SetProjectionMode(ProjectionMode::Perspective);
        Camera->SetFieldOfView(60.0f);
        CameraActor->SetPosition(Float3 {0.0f, 1.0f, 4.0f});

        const ActorHandle LightHandle = TestScene.Spawn("Light");
        Actor* LightActor             = TestScene.Get(LightHandle);
        auto* Light                   = LightActor->AddComponent<DirectionalLightComponent>();
        Light->SetIntensity(1.0f);
        using namespace DirectX;
        const XMVECTOR LightRotation =
          XMQuaternionRotationRollPitchYaw(XMConvertToRadians(-45.0f), XMConvertToRadians(150.0f), 0.0f);
        Quat LightRotationOut;
        XMStoreFloat4(&LightRotationOut, LightRotation);
        LightActor->SetRotation(LightRotationOut);

        const ActorHandle EnvironmentHandle = TestScene.Spawn("Environment");
        Actor* EnvironmentActor             = TestScene.Get(EnvironmentHandle);
        auto* Environment                   = EnvironmentActor->AddComponent<EnvironmentComponent>();
        Environment->SetMapAsset(ASSET("ibl/maps/sky_spring.hdr"));

        // LoadSceneFromFile reads a plain filesystem path directly (not
        // through the AssetRegistry/mount system - see Game::
        // ApplyPendingSceneChange), so this doesn't need to live inside any
        // mounted content dir - keeping it out of Sandbox's own Content/
        // avoids polluting that tree with an XED-only scratch file.
        const std::filesystem::path ScratchPath = "xed_test_scene.xscene";
        SceneSerializer::SaveToFile(TestScene, ScratchPath);
        return ScratchPath;
    }

    Editor::~Editor() {
        if (_EditorWindow) _EditorWindow->SetDebugUI(nullptr);
    }

    void Editor::Run() {
        _Running = true;

        using Clock   = std::chrono::steady_clock;
        auto Previous = Clock::now();

        while (_Running && !_EditorWindow->ShouldClose()) {
            const auto Now = Clock::now();
            f32 Delta      = std::chrono::duration<f32>(Now - Previous).count();
            Previous       = Now;
            if (Delta > 0.25f) Delta = 0.25f;

            TickFrame(Delta);

            _EditorWindow->PollEvents();
        }
    }

    void Editor::TickFrame(const f32 DeltaTime) {
        if (_EditorWindow->IsMinimized()) return;

        // The other half of the swap chain's own resize wiring - mirrors
        // Game::TickFrame's identical call for the standalone path.
        // Window::ConsumeResized is edge-triggered, so this has to run every
        // tick or a resize is lost. Without it the swap chain's back
        // buffers stay whatever size they were created at while the OS
        // window itself grows or shrinks - a flip-model swap chain just
        // stretches that stale-sized back buffer to fill the new client
        // area on Present, which is exactly "resizing the window doesn't
        // resize the content" (the dock layout itself already tracks the
        // real window size fine - ImGui reads that straight from Win32,
        // independent of the swap chain).
        if (_EditorWindow->ConsumeResized()) {
            _Device->SetSwapChainSize(_EditorWindow->GetWidth(), _EditorWindow->GetHeight());
        }

        // Exactly one BeginFrame/EndFrame bracket for the whole tick,
        // shared by both the embedded Game's own scene render (see
        // DrawDockspaceAndPanels, which calls TickEmbedded once the "Scene"
        // panel's size for this frame is already applied) and this
        // editor's own UI. Game::TickFrame no longer opens its own bracket
        // in embedded mode - two BeginFrame/EndFrame pairs per tick each
        // present the swap chain once, and the first one had nothing ever
        // drawn into it, which is what produced a visible flicker as soon
        // as a scene became active.
        _Device->BeginFrame();

        // The editor's own "base layer": a plain clear of the swap chain,
        // exactly the same RenderPassDesc::SwapChain helper LoadingScreen
        // already uses. DebugUI::EndFrame below draws the whole editor UI as
        // an overlay on top of this - there's no other "game content" to
        // put in the back buffer directly, since the actual scene lives
        // inside the "Scene" panel's ImGui::Image instead.
        _Commands.Reset();
        _Commands.BeginRenderPass(RHI::RenderPassDesc::SwapChain(0.08f, 0.08f, 0.08f, 1.0f));
        _Commands.EndRenderPass();
        _Device->Submit(_Commands);

        _DebugUI.BeginFrame();
        DrawDockspaceAndPanels(DeltaTime);
        _DebugUI.EndFrame();

        _Device->EndFrame();
    }

    void Editor::EnsureDefaultLayout(const unsigned int DockspaceID) const {
        if (ImGui::DockBuilderGetNode(DockspaceID)) return;  // a saved layout already exists

        ImGui::DockBuilderRemoveNode(DockspaceID);
        ImGui::DockBuilderAddNode(DockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(DockspaceID, ImGui::GetMainViewport()->Size);

        ImGuiID Center       = DockspaceID;
        const ImGuiID Right  = ImGui::DockBuilderSplitNode(Center, ImGuiDir_Right, 0.25f, nullptr, &Center);
        const ImGuiID Left   = ImGui::DockBuilderSplitNode(Center, ImGuiDir_Left, 0.20f, nullptr, &Center);
        const ImGuiID Bottom = ImGui::DockBuilderSplitNode(Center, ImGuiDir_Down, 0.25f, nullptr, &Center);

        ImGui::DockBuilderDockWindow("Scene", Center);
        ImGui::DockBuilderDockWindow("Hierarchy", Left);
        ImGui::DockBuilderDockWindow("Inspector", Right);
        ImGui::DockBuilderDockWindow("Content Browser", Bottom);
        ImGui::DockBuilderDockWindow("Log", Bottom);

        ImGui::DockBuilderFinish(DockspaceID);
    }

    void Editor::DrawDockspaceAndPanels(const f32 DeltaTime) {
        const ImGuiID DockspaceID = ImGui::GetID("EditorDockspace");
        EnsureDefaultLayout(DockspaceID);
        ImGui::DockSpaceOverViewport(DockspaceID, ImGui::GetMainViewport());

        if (ImGui::Begin("Scene")) {
            const ImVec2 Avail   = ImGui::GetContentRegionAvail();
            const auto NewWidth  = CAST<u32>(std::max(Avail.x, 1.0f));
            const auto NewHeight = CAST<u32>(std::max(Avail.y, 1.0f));
            if (NewWidth != _SceneViewportWidth || NewHeight != _SceneViewportHeight) {
                _SceneViewportWidth  = NewWidth;
                _SceneViewportHeight = NewHeight;
                _EmbeddedGame->SetViewport(NewWidth, NewHeight);
            }

            // Rendered here, now that any resize above has already landed -
            // so the texture sampled below always reflects a frame actually
            // rendered at THIS size. Calling this before the resize (the
            // original ordering) meant every size-changing frame threw away
            // the frame just rendered the instant Resize ran, since
            // Viewport::Resize destroys and recreates the color target
            // immediately - visible as the "Scene" panel doing nothing
            // while a splitter drag was in progress.
            _EmbeddedGame->TickEmbedded(DeltaTime);

            const ImTextureID SceneTexture =
              _DebugUI.GetOrCreateSceneTextureID(_EmbeddedGame->GetMainViewport().GetColorTarget());
            if (SceneTexture != 0) { ImGui::Image(SceneTexture, Avail); }
        }
        ImGui::End();

        if (ImGui::Begin("Hierarchy")) { ImGui::TextDisabled("(scene hierarchy - later work)"); }
        ImGui::End();

        if (ImGui::Begin("Inspector")) { ImGui::TextDisabled("(selected-actor properties - later work)"); }
        ImGui::End();

        if (ImGui::Begin("Content Browser")) { ImGui::TextDisabled("(project content - later work)"); }
        ImGui::End();

        if (ImGui::Begin("Log")) { ImGui::TextDisabled("(engine log - later work)"); }
        ImGui::End();
    }
}  // namespace Xen
