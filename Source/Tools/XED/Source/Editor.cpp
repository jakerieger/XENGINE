//
// Created by Jake Rieger on 9/27/2026.
//

#include "Editor.hpp"

#include <Xen/XenGameSettings.h>
#include <Xen/SceneSerializer.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <chrono>

#pragma region Embedded Resources
#include "Resource/InterRegular.h"
#pragma endregion

namespace Xen {
    namespace {
        struct EditorState {
            std::vector<std::string> SceneActors;
            int SelectedActor = 0;
            bool ActorEnabled {false};
            Float3 TransformPosition {};
            Float3 TransformRotation {};
            Float3 TransformScale {};
        };
    }  // namespace

    // Global frame-by-frame UI state
    static EditorState State {};

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

        if (!_UI.Initialize(*_Device, *_EditorWindow)) {
            THROW_ENGINE_EXCEPTION(EditorException, "failed to initialize editor UI");
        }
        _EditorWindow->SetUIOverlay(&_UI);

        if (!_UI.LoadFont("inter", INTERREGULAR_TTF_BYTES, INTERREGULAR_TTF_SIZE, 16.0f)) {
            THROW_ENGINE_EXCEPTION(EditorException, "failed to load font");
        }

        // Sized once here to something reasonable; the "Scene" panel's own
        // content-region size takes over from the first real layout pass
        // (see DrawDockspaceAndPanels).
        _SceneViewportWidth  = 1280;
        _SceneViewportHeight = 720;

        _Config = EditorConfig::Read("Config/EditorConfig.ini");
        LOG_INFO("EditorConfig:\n - CurrentProject: %s\n - StartupMode: %d\n - UITheme: %s",
                 _Config.CurrentProject.string().c_str(),
                 static_cast<u32>(_Config.StartupMode),
                 _Config.UITheme.c_str());

        if (_Config.StartupMode == EditorStartupMode::Maximized) {
            ::PostMessageA(_EditorWindow->GetHandle(), WM_SYSCOMMAND, SC_MAXIMIZE, 0);
        }

        // We'll check that it exists here even though LoadProject already checks to avoid throwing an exception if it
        // doesn't. The editor should still start if the startup project is invalid and just prompt the user to select
        // or create a new project to load. Later, a flag of some kind will be added that tells the editor this failed.
        if (!_Config.CurrentProject.empty() && std::filesystem::exists(_Config.CurrentProject)) {
            LoadProject(_Config.CurrentProject);
        } else {
            ::MessageBoxA(_EditorWindow->GetHandle(),
                          "No startup scene defined in EditorConfig.ini",
                          "XED",
                          MB_OK | MB_ICONWARNING);
        }
    }

    Editor::~Editor() {
        if (_EditorWindow) _EditorWindow->SetUIOverlay(nullptr);
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

    void Editor::LoadProject(const std::filesystem::path& PrxjPath) {
        if (!exists(PrxjPath)) { THROW_ENGINE_EXCEPTION(EditorException, "Project does not exist"); }

        const auto LoadResult = ProjectSerializer::LoadFromFile(PrxjPath);
        if (!LoadResult.has_value()) {
            THROW_ENGINE_EXCEPTION(EditorException, "Failed to load project (error during parsing)");
        }
        _CurrentProject = *LoadResult;

        PAK::AssetMountConfig MountConfig = BuildMountConfig(Xen::Generated::GameSettings(), 0, nullptr);
        MountConfig.ContentDirs           = {_CurrentProject.ContentDirectory};

        _EmbeddedGame.reset(
          new Game(*_Device, MountConfig, _SceneViewportWidth, _SceneViewportHeight, _CurrentProject.ConfigDirectory));
        const auto& EngineConfig = _EmbeddedGame->_EngineConfig;
        if (!EngineConfig.StartupScene.empty()) {
            _EmbeddedGame->LoadSceneFromFile(_CurrentProject.ContentDirectory / EngineConfig.StartupScene);
        }

        _EmbeddedGame->StartEmbedded();
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
        // already uses. EditorUI::EndFrame below draws the whole editor UI
        // as an overlay on top of this - there's no other "game content" to
        // put in the back buffer directly, since the actual scene lives
        // inside the "Scene" panel's ImGui::Image instead.
        _Commands.Reset();
        _Commands.BeginRenderPass(RHI::RenderPassDesc::SwapChain(0.08f, 0.08f, 0.08f, 1.0f));
        _Commands.EndRenderPass();
        _Device->Submit(_Commands);

        _UI.BeginFrame();
        DrawDockspaceAndPanels(DeltaTime);
        _UI.EndFrame();

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

    void Editor::View_Inspector() const {
        if (ImGui::Begin("Inspector")) {
            if (_EmbeddedGame && _EmbeddedGame->GetActiveScene()) {
                const auto* S = _EmbeddedGame->GetActiveScene();
                if (S) {
                    const ActorHandle SelectedActorHandle = S->FindByActorID(State.SelectedActor + 1);
                    Actor* pSelectedActor                 = S->Get(SelectedActorHandle);
                    if (pSelectedActor) {
                        ImGui::Text("%s", pSelectedActor->GetName().c_str());

                        State.ActorEnabled = pSelectedActor->IsEnabled();
                        ImGui::Checkbox("Enabled", &State.ActorEnabled);
                        pSelectedActor->SetEnabled(State.ActorEnabled);

                        State.TransformPosition = pSelectedActor->GetWorldTransform().Position;
                        const auto RotationQuat = pSelectedActor->GetWorldTransform().Rotation;
                        const auto EulerAngles  = QuaternionToEuler(RotationQuat);
                        State.TransformRotation = {DirectX::XMConvertToDegrees(EulerAngles.x),
                                                   DirectX::XMConvertToDegrees(EulerAngles.y),
                                                   DirectX::XMConvertToDegrees(EulerAngles.z)};
                        State.TransformScale    = pSelectedActor->GetWorldTransform().Scale;

                        ImGui::DragFloat3("Position", &State.TransformPosition.x, 0.01f);
                        ImGui::DragFloat3("Rotation", &State.TransformRotation.x, 0.1f);
                        ImGui::DragFloat3("Scale", &State.TransformScale.x, 0.01f);

                        pSelectedActor->SetPosition(State.TransformPosition);
                        // Rotating on X axis mostly works, the other two axes just snap back to zero.
                        const Float3 NewRotation = {
                          DirectX::XMConvertToRadians(State.TransformRotation.x),
                          DirectX::XMConvertToRadians(State.TransformRotation.y),
                          DirectX::XMConvertToRadians(State.TransformRotation.z),
                        };
                        pSelectedActor->SetRotation(EulerToQuaternion(NewRotation));
                        pSelectedActor->SetScale(State.TransformScale);
                    }
                }
            }
        }
        ImGui::End();
    }

    void Editor::View_Scene(const f32 DeltaTime) {
        if (ImGui::Begin("Scene")) {
            const ImVec2 Avail   = ImGui::GetContentRegionAvail();
            const auto NewWidth  = CAST<u32>(std::max(Avail.x, 1.0f));
            const auto NewHeight = CAST<u32>(std::max(Avail.y, 1.0f));
            if (NewWidth != _SceneViewportWidth || NewHeight != _SceneViewportHeight) {
                _SceneViewportWidth  = NewWidth;
                _SceneViewportHeight = NewHeight;
                if (_EmbeddedGame) _EmbeddedGame->SetViewport(NewWidth, NewHeight);
            }

            // Rendered here, now that any resize above has already landed -
            // so the texture sampled below always reflects a frame actually
            // rendered at THIS size. Calling this before the resize (the
            // original ordering) meant every size-changing frame threw away
            // the frame just rendered the instant Resize ran, since
            // Viewport::Resize destroys and recreates the color target
            // immediately - visible as the "Scene" panel doing nothing
            // while a splitter drag was in progress.
            if (_EmbeddedGame) _EmbeddedGame->TickEmbedded(DeltaTime);

            const ImTextureID SceneTexture =
              _EmbeddedGame ? _UI.GetOrCreateSceneTextureID(_EmbeddedGame->GetMainViewport().GetColorTarget()) : 0;
            if (SceneTexture != 0) { ImGui::Image(SceneTexture, Avail); }
        }
        ImGui::End();
    }
    void Editor::View_Hierarchy() const {
        if (ImGui::Begin("Hierarchy")) {
            if (_EmbeddedGame && _EmbeddedGame->GetActiveScene()) {
                const auto* S = _EmbeddedGame->GetActiveScene();
                if (S) {
                    State.SceneActors.clear();

                    S->ForEachActor([&](const Actor& A) { State.SceneActors.emplace_back(A.GetName()); });

                    if (ImGui::BeginListBox("##Actors", ImVec2(-FLT_MIN, -FLT_MIN))) {
                        for (auto i = 0; i < State.SceneActors.size(); i++) {
                            const bool IsSelected = (State.SelectedActor == i);
                            if (ImGui::Selectable(State.SceneActors[i].c_str(), IsSelected)) {
                                State.SelectedActor = i;
                            }

                            if (IsSelected) ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndListBox();
                }
            }
        }
        ImGui::End();
    }

    void Editor::View_ContentBrowser() const {
        if (ImGui::Begin("Content Browser")) { ImGui::TextDisabled("(project content - later work)"); }
        ImGui::End();
    }

    void Editor::View_Log() const {
        if (ImGui::Begin("Log")) { ImGui::TextDisabled("(engine log - later work)"); }
        ImGui::End();
    }

    void Editor::DrawDockspaceAndPanels(const f32 DeltaTime) {
        const ImGuiID DockspaceID = ImGui::GetID("EditorDockspace");
        EnsureDefaultLayout(DockspaceID);
        ImGui::DockSpaceOverViewport(DockspaceID, ImGui::GetMainViewport());

        View_Scene(DeltaTime);
        View_Hierarchy();
        View_Inspector();
        View_ContentBrowser();
        View_Log();
    }
}  // namespace Xen
