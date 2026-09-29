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
#include "Resource/InterBold.h"
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

    void Editor::LoadEditorFonts() const {
        if (!_UI.LoadFont("InterRegular", INTERREGULAR_TTF_DATA, INTERREGULAR_TTF_SIZE, 16.0f)) {
            THROW_ENGINE_EXCEPTION(EditorException, "failed to load font");
        }
        if (!_UI.LoadFont("InterBold", INTERBOLD_TTF_DATA, INTERBOLD_TTF_SIZE, 16.0f)) {
            THROW_ENGINE_EXCEPTION(EditorException, "failed to load font");
        }
    }

    Editor::Editor() {
        _Window = std::make_unique<Window>("XED", EngineConfig::WindowMode::Windowed, 1600, 900);
        if (!_Window) { THROW_ENGINE_EXCEPTION(EditorException, "failed to create editor window"); }

        _Device = RHI::CreateRenderDevice(RHI::Backend::D3D12);
        if (!_Device) { THROW_ENGINE_EXCEPTION(EditorException, "no render device for the requested backend"); }

        RHI::DeviceDescriptor Descriptor {};
        Descriptor.NativeWindowHandle = _Window->GetHandle();
#ifndef NDEBUG
        Descriptor.EnableValidation   = true;
        Descriptor.EnableDebugMarkers = true;
#endif
        if (!_Device->Initialize(Descriptor)) {
            THROW_ENGINE_EXCEPTION(EditorException, "failed to initialize render device");
        }
        _Device->SetSwapChainSize(_Window->GetWidth(), _Window->GetHeight());

        if (!_UI.Initialize(*_Device, *_Window)) {
            THROW_ENGINE_EXCEPTION(EditorException, "failed to initialize editor UI");
        }
        _Window->SetUIOverlay(&_UI);

        LoadEditorFonts();

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

        if (_Config.StartupMode == EditorStartupMode::Maximized) _Window->Maximize();
        if (!_Config.UITheme.empty()) LoadTheme(_Config.UITheme);

        // We'll check that it exists here even though LoadProject already checks to avoid throwing an exception if it
        // doesn't. The editor should still start if the startup project is invalid and just prompt the user to select
        // or create a new project to load. Later, a flag of some kind will be added that tells the editor this failed.
        if (!_Config.CurrentProject.empty() && std::filesystem::exists(_Config.CurrentProject)) {
            LoadProject(_Config.CurrentProject);
        } else {
            ::MessageBoxA(_Window->GetHandle(),
                          "No startup scene defined in EditorConfig.ini",
                          "XED",
                          MB_OK | MB_ICONWARNING);
        }
    }

    Editor::~Editor() {
        if (_Window) _Window->SetUIOverlay(nullptr);
    }

    void Editor::Run() {
        _Running = true;

        using Clock   = std::chrono::steady_clock;
        auto Previous = Clock::now();

        while (_Running && !_Window->ShouldClose()) {
            const auto Now = Clock::now();
            f32 Delta      = std::chrono::duration<f32>(Now - Previous).count();
            Previous       = Now;
            if (Delta > 0.25f) Delta = 0.25f;

            TickFrame(Delta);

            _Window->PollEvents();
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
        if (_Window->IsMinimized()) return;
        if (_Window->ConsumeResized()) { _Device->SetSwapChainSize(_Window->GetWidth(), _Window->GetHeight()); }

        _Device->BeginFrame();

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

    void Editor::LoadTheme(const std::string& ThemeFile) {
        const auto ThemePath = std::filesystem::current_path() / "Config" / "Themes" / ThemeFile;
        if (!exists(ThemePath)) { THROW_ENGINE_EXCEPTION(EditorException, "Theme does not exist"); }

        const auto LoadResult = ThemeSerializer::LoadFromFile(ThemePath);
        if (!LoadResult.has_value()) {
            THROW_ENGINE_EXCEPTION(EditorException, "Failed to load theme (error during parsing)");
        }
        _CurrentTheme = *LoadResult;

        ApplyCurrentTheme();
    }

    void Editor::ApplyCurrentTheme() const {
        ImGuiStyle& Style = ImGui::GetStyle();
        ImVec4* Colors    = Style.Colors;

        Style.WindowRounding   = _CurrentTheme.WindowRounding;
        Style.FrameRounding    = _CurrentTheme.FrameRounding;
        Style.TabRounding      = _CurrentTheme.TabRounding;
        Style.WindowBorderSize = _CurrentTheme.WindowBorderSize;
        Style.FrameBorderSize  = _CurrentTheme.FrameBorderSize;

        Colors[ImGuiCol_BorderShadow]   = ImVec4(0.f, 0.f, 0.f, 0.f);
        Colors[ImGuiCol_Border]         = _CurrentTheme.Colors.Border.To<ImVec4>();
        Colors[ImGuiCol_ButtonActive]   = _CurrentTheme.Colors.ButtonPrimary.WithAlpha(0.67f).To<ImVec4>();
        Colors[ImGuiCol_ButtonHovered]  = _CurrentTheme.Colors.ButtonPrimary.WithAlpha(0.8f).To<ImVec4>();
        Colors[ImGuiCol_Button]         = _CurrentTheme.Colors.ButtonPrimary.To<ImVec4>();
        Colors[ImGuiCol_CheckMark]      = _CurrentTheme.Colors.TextPrimary.To<ImVec4>();
        Colors[ImGuiCol_ChildBg]        = _CurrentTheme.Colors.PanelBackground.To<ImVec4>();
        Colors[ImGuiCol_DockingPreview] = _CurrentTheme.Colors.TextPrimary.To<ImVec4>();
        Colors[ImGuiCol_DragDropTarget] = _CurrentTheme.Colors.TextPrimary.To<ImVec4>();
        Colors[ImGuiCol_FrameBgActive]  = _CurrentTheme.Colors.Input.WithAlpha(0.4f).To<ImVec4>();
        Colors[ImGuiCol_FrameBgHovered] = _CurrentTheme.Colors.Input.WithAlpha(0.7f).To<ImVec4>();
        Colors[ImGuiCol_FrameBg]        = _CurrentTheme.Colors.Input.To<ImVec4>();
        Colors[ImGuiCol_HeaderActive] =
          _CurrentTheme.Colors.WindowBackground.WithAlpha(0.67f).To<ImVec4>();  // Selected item in listbox
        Colors[ImGuiCol_HeaderHovered]         = _CurrentTheme.Colors.WindowBackground.WithAlpha(0.8f).To<ImVec4>();
        Colors[ImGuiCol_Header]                = _CurrentTheme.Colors.WindowBackground.To<ImVec4>();
        Colors[ImGuiCol_MenuBarBg]             = _CurrentTheme.Colors.Input.To<ImVec4>();
        Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.5f);
        Colors[ImGuiCol_NavHighlight]          = ImVec4(30.f / 255.f, 30.f / 255.f, 30.f / 255.f, 1.00f);
        Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        Colors[ImGuiCol_PlotLines]             = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        Colors[ImGuiCol_PopupBg]               = _CurrentTheme.Colors.WindowBackground.To<ImVec4>();
        Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
        Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
        Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
        Colors[ImGuiCol_ScrollbarBg]           = _CurrentTheme.Colors.Input.To<ImVec4>();
        Colors[ImGuiCol_ScrollbarGrabActive]   = _CurrentTheme.Colors.TextPrimary.To<ImVec4>();
        Colors[ImGuiCol_ScrollbarGrabHovered]  = _CurrentTheme.Colors.TextPrimary.To<ImVec4>();
        Colors[ImGuiCol_ScrollbarGrab]         = _CurrentTheme.Colors.TextPrimary.To<ImVec4>();
        Colors[ImGuiCol_SeparatorActive]       = _CurrentTheme.Colors.TextPrimary.To<ImVec4>();
        Colors[ImGuiCol_SeparatorHovered]      = _CurrentTheme.Colors.TextPrimary.To<ImVec4>();
        Colors[ImGuiCol_Separator]             = Color("#4e4e4e").To<ImVec4>();
        Colors[ImGuiCol_SliderGrabActive]      = _CurrentTheme.Colors.TextPrimary.To<ImVec4>();
        Colors[ImGuiCol_SliderGrab]            = _CurrentTheme.Colors.TextPrimary.To<ImVec4>();
        Colors[ImGuiCol_TabActive]             = _CurrentTheme.Colors.TabActive.To<ImVec4>();
        Colors[ImGuiCol_TabHovered]            = _CurrentTheme.Colors.TabActive.To<ImVec4>();
        Colors[ImGuiCol_TabUnfocusedActive]    = Colors[ImGuiCol_TabActive];
        Colors[ImGuiCol_TabUnfocused]          = Colors[ImGuiCol_Tab];
        Colors[ImGuiCol_Tab]                   = _CurrentTheme.Colors.TabInactive.To<ImVec4>();
        Colors[ImGuiCol_TableBorderLight]      = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);  // Prefer using Alpha=1.0 here
        Colors[ImGuiCol_TableBorderLight]      = ImVec4(0.f, 0.f, 0.f, 0.f);
        Colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);  // Prefer using Alpha=1.0 here
        Colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.f, 0.f, 0.f, 0.f);
        Colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
        Colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
        Colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        Colors[ImGuiCol_TextDisabled]          = _CurrentTheme.Colors.TextSecondary.To<ImVec4>();
        Colors[ImGuiCol_TextSelectedBg]        = _CurrentTheme.Colors.TextPrimary.WithAlpha(0.5f).To<ImVec4>();
        Colors[ImGuiCol_Text]                  = _CurrentTheme.Colors.TextPrimary.To<ImVec4>();
        Colors[ImGuiCol_TitleBgActive]         = _CurrentTheme.Colors.PanelBackground.To<ImVec4>();
        Colors[ImGuiCol_TitleBgCollapsed]      = _CurrentTheme.Colors.PanelBackground.To<ImVec4>();
        Colors[ImGuiCol_TitleBg]               = _CurrentTheme.Colors.PanelBackground.To<ImVec4>();
        Colors[ImGuiCol_WindowBg]              = _CurrentTheme.Colors.WindowBackground.To<ImVec4>();
    }

    void Editor::View_Inspector() const {
        if (ImGui::Begin("Inspector")) {
            if (_EmbeddedGame && _EmbeddedGame->GetActiveScene()) {
                const auto* S = _EmbeddedGame->GetActiveScene();
                if (S) {
                    const ActorHandle SelectedActorHandle = S->FindByActorID(State.SelectedActor + 1);
                    Actor* pSelectedActor                 = S->Get(SelectedActorHandle);
                    if (pSelectedActor) {
                        {
                            ScopedFont _(&_UI, "InterBold");
                            ImGui::Text("%s", pSelectedActor->GetName().c_str());
                        }

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

                        // ===========================================================

                        {
                            ScopedFont _(&_UI, "InterBold");
                            if (ImGui::Button("Add Component", ImVec2(-FLT_MIN, 32.f))) {}
                        }
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
