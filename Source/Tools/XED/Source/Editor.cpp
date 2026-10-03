//
// Created by Jake Rieger on 9/27/2026.
//

#include "Editor.hpp"
#include "PropertyEditor.hpp"
#include "ProjectFileTemplate.hpp"

#include "UI.hpp"

#include <Common/Io.hpp>
#include <Common/Log.hpp>
#include <Xen/XenGameSettings.h>
#include <Xen/SceneSerializer.hpp>

#pragma region Components
#include <Xen/Components/AmbientOcclusionComponent.hpp>
#include <Xen/Components/AntiAliasingComponent.hpp>
#include <Xen/Components/AudioSourceComponent.hpp>
#include <Xen/Components/CameraComponent.hpp>
#include <Xen/Components/DirectionalLightComponent.hpp>
#include <Xen/Components/EnvironmentComponent.hpp>
#include <Xen/Components/FPPlayerController.hpp>
#include <Xen/Components/MeshComponent.hpp>
#include <Xen/Components/PointLightComponent.hpp>
#include <Xen/Components/PostProcessComponent.hpp>
#include <Xen/Components/SpotLightComponent.hpp>
#include <Xen/Components/SpriteComponent.hpp>
#pragma endregion

#include <algorithm>
#include <chrono>
#include <memory>
#include <Lmcons.h>  // contains UNLEN (maximum length of Windows username)

#pragma region Embedded Resources
#include <Xen/MaterialBindings.hpp>
#include "Resource/InterRegular.h"
#include "Resource/InterBold.h"
#pragma endregion

namespace Xen {

    namespace {
        // Name shown in the Hierarchy list alongside the actual handle it
        // refers to - see EditorState::SelectedActor's own comment for why
        // the handle, not a list position, is what actually identifies an
        // actor here.
        struct ActorListEntry {
            std::string Name;
            ActorHandle Handle;
        };

        struct EditorState {
            std::vector<ActorListEntry> SceneActors;

            // The actually-selected actor, by handle - NOT a list index.
            // Scene::ForEachActor iterates actors in slot order, and a
            // deleted actor's slot gets reused (LIFO) by the next spawn, so
            // "index i" can refer to a completely different actor from one
            // frame to the next once anything has ever been deleted -
            // exactly the scenario Duplicate -> Delete the copy -> Duplicate
            // the original again hits, since the second Duplicate's clone
            // lands in the first clone's just-freed slot. ActorHandle
            // doesn't have this problem: Scene bumps a slot's generation on
            // destroy specifically so a handle into a reused slot fails to
            // resolve instead of silently resolving to whichever new actor
            // inherited it (see Scene.hpp's own class comment).
            ActorHandle SelectedActor {};

            bool ActorEnabled {false};
            Float3 TransformPosition {};
            Float3 TransformRotation {};
            Float3 TransformScale {};
            std::array<char, MAX_PATH> NewActorName {'\0'};

            // Modal flags
            bool ShowNewProjectModal {false};
            bool ShowNewSceneModal {false};

            fs::path CurrentSceneFile {};
        };

        constexpr f32 ToolbarHeight = 40.0f;
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

    Editor::Editor() : _SettingsModal(_EditorSettings) {
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

        // Not fatal if this fails partway through - see IconLibrary::
        // Initialize's own comment. A toolbar button just draws without an
        // icon (DrawToolbar treats ImTextureID 0 as "skip it").
        if (!_Icons.Initialize(*_Device, _UI)) { LOG_ERR("Editor: failed to initialize icon library"); }

        LoadEditorFonts();
        SetupShortcuts();

        // Sized once here to something reasonable; the "Scene" panel's own
        // content-region size takes over from the first real layout pass
        // (see DrawDockspaceAndPanels).
        _SceneViewportWidth  = 1280;
        _SceneViewportHeight = 720;

        _EditorSettings = EditorSettings::Read("Config/EditorSettings.ini");
        if (_EditorSettings.StartupMode == EditorStartupMode::Maximized) _Window->Maximize();
        if (!_EditorSettings.UITheme.empty()) LoadTheme(_EditorSettings.UITheme);

        // We'll check that it exists here even though LoadProject already checks to avoid throwing an exception if it
        // doesn't. The editor should still start if the startup project is invalid and just prompt the user to select
        // or create a new project to load. Later, a flag of some kind will be added that tells the editor this failed.
        if (!_EditorSettings.CurrentProject.empty() && fs::exists(_EditorSettings.CurrentProject)) {
            LoadProject(_EditorSettings.CurrentProject);
        }
        // else {
        //     Modal_NewProject();
        // }
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

    void Editor::LoadProject(const fs::path& PrxjPath) {
        if (!exists(PrxjPath)) { THROW_ENGINE_EXCEPTION(EditorException, "Project does not exist"); }

        const auto LoadResult = ProjectSerializer::LoadFromFile(PrxjPath);
        if (!LoadResult.has_value()) {
            THROW_ENGINE_EXCEPTION(EditorException, "Failed to load project (error during parsing)");
        }
        _CurrentProject = *LoadResult;

        PAK::AssetMountConfig MountConfig = BuildMountConfig(Xen::Generated::GameSettings(), 0, nullptr);
        MountConfig.ContentDirs           = {_CurrentProject.ContentDirectory};

        _EmbeddedGame = std::make_unique<Game>(*_Device,
                                               MountConfig,
                                               _SceneViewportWidth,
                                               _SceneViewportHeight,
                                               _CurrentProject.ConfigDirectory);
        _EmbeddedGame->StartEmbedded();

        SetWindowTitle(_CurrentProject.Name);

        const auto& EngineConfig = _EmbeddedGame->_EngineConfig;
        if (!EngineConfig.StartupScene.empty()) {
            const auto SceneFilePath = _CurrentProject.ContentDirectory / EngineConfig.StartupScene;
            LoadSceneFile(SceneFilePath);
        }
    }

    Editor::CreateProjectResult Editor::CreateProject(const std::string& Name, const fs::path& Dir) {
        EditorProject Project;
        Project.Name             = Name;
        Project.ProjectRoot      = Dir;
        Project.Version          = XED_PROJECT_FORMAT_VERSION;
        Project.ConfigDirectory  = "Config";
        Project.ContentDirectory = "Content";
        Project.RuntimeDirectory = "Runtime";

        // TODO: Create project directories/files and serialize project to file
        if (exists(Dir)) { return CreateProjectResult::AlreadyExists; }
        if (!fs::create_directories(Dir)) { return CreateProjectResult::Failed; }

        const auto ConfigDir = Dir / "Config";
        if (!fs::create_directories(ConfigDir)) { return CreateProjectResult::Failed; }

        // Copy config templates to new project config dir
        if (!fs::copy_file("Templates/Config/AudioConfig.ini", ConfigDir / "AudioConfig.ini")) {
            return CreateProjectResult::Failed;
        }
        if (!fs::copy_file("Templates/Config/EngineConfig.ini", ConfigDir / "EngineConfig.ini")) {
            return CreateProjectResult::Failed;
        }
        if (!fs::copy_file("Templates/Config/InputConfig.ini", ConfigDir / "InputConfig.ini")) {
            return CreateProjectResult::Failed;
        }

        const auto ContentDir = Dir / "Content";
        if (!fs::create_directories(ContentDir)) { return CreateProjectResult::Failed; }

        const auto RuntimeDir = Dir / "Runtime";
        if (!fs::create_directories(RuntimeDir)) { return CreateProjectResult::Failed; }

        // Create runtime source files
        const auto CMakeListsTxtPath = Dir / "CMakeLists.txt";
        const auto MainCppPath       = RuntimeDir / "main.cpp";
        const auto GameClassCppPath  = (RuntimeDir / Name).replace_extension(".cpp");
        const auto GameClassHppPath  = (RuntimeDir / Name).replace_extension(".hpp");

        try {
            char Username[UNLEN + 1];
            DWORD UsernameLen = UNLEN + 1;
            if (!::GetUserNameA(Username, &UsernameLen)) {
                LOG_ERR("Failed to get user name");
                strcpy_s(Username, UsernameLen, "Unknown");
            }

            std::unordered_map<std::string, std::string> TemplateVars = {
              {"GAME_CLASS", Name},
              {"USER", Username},
              {"DATE", DateTime::Now().DateString()},
            };

            auto CMakeListsTemplate  = IO::ReadString("Templates/CMakeLists.txt");
            const auto CMakeListsTxt = ParseTemplate(CMakeListsTemplate, TemplateVars);
            IO::WriteString(CMakeListsTxt, CMakeListsTxtPath);

            auto MainCppTemplate = IO::ReadString("Templates/Runtime/main.cpp");
            const auto MainCpp   = ParseTemplate(MainCppTemplate, TemplateVars);
            IO::WriteString(MainCpp, MainCppPath);

            auto GameClassCppTemplate = IO::ReadString("Templates/Runtime/GameClass.cpp");
            const auto GameClassCpp   = ParseTemplate(GameClassCppTemplate, TemplateVars);
            IO::WriteString(GameClassCpp, GameClassCppPath);

            auto GameClassHppTemplate = IO::ReadString("Templates/Runtime/GameClass.hpp");
            const auto GameClassHpp   = ParseTemplate(GameClassHppTemplate, TemplateVars);
            IO::WriteString(GameClassHpp, GameClassHppPath);
        } catch (...) { return CreateProjectResult::Failed; }

        const auto PrxjPath = (Dir / Name).replace_extension(".prxj");
        try {
            ProjectSerializer::SaveToFile(Project, PrxjPath);
        } catch (...) { return CreateProjectResult::Failed; }

        return CreateProjectResult::Success;
    }

    void Editor::LoadSceneFile(const fs::path& SceneFile) const {
        if (!_EmbeddedGame || !_EmbeddedGame->IsRunning()) { return; }
        _EmbeddedGame->LoadSceneFromFile(SceneFile);
        State.CurrentSceneFile = SceneFile;
        SetWindowTitle(std::format("{} ({})", canonical(SceneFile).filename().string(), _CurrentProject.Name));
    }

    void Editor::CreateScene(const std::string& Name, const fs::path& SceneFile) const {
        if (!_EmbeddedGame || !_EmbeddedGame->IsRunning()) { return; }

        Scene NewScene(Name);
        NewScene.SetContext(_EmbeddedGame->GetContext());

        const auto CameraHandle = NewScene.Spawn("MainCamera");
        auto* CameraActor       = NewScene.Get(CameraHandle);
        if (CameraActor) {
            auto* Camera = CameraActor->AddComponent<CameraComponent>();
            if (Camera) {
                Camera->SetProjectionMode(ProjectionMode::Perspective);
                Camera->SetFieldOfView(60.0f);
            }

            CameraActor->SetPosition(Float3(0.0f, 1.0f, 10.0f));
        }

        const auto LightHandle = NewScene.Spawn("Sun");
        auto* LightActor       = NewScene.Get(LightHandle);
        if (LightActor) {
            auto* Light = LightActor->AddComponent<DirectionalLightComponent>();
            if (Light) {
                Light->SetIntensity(1.0f);
                Light->SetShadowDistance(15.0f);
            }

            const DirectX::XMVECTOR LightRotation =
              DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(-45.0f),
                                                        DirectX::XMConvertToRadians(150.0f),
                                                        0.0f);
            Quat LightRotationOut;
            XMStoreFloat4(&LightRotationOut, LightRotation);
            LightActor->SetRotation(LightRotationOut);
        }

        const auto EnvHandle = NewScene.Spawn("Environment");
        auto* EnvActor       = NewScene.Get(EnvHandle);
        if (EnvActor) {
            auto* Env = EnvActor->AddComponent<EnvironmentComponent>();
            if (Env) {
                Env->SetMapAsset(ASSET("ibl/maps/sky_spring.hdr"));
                Env->SetShowBackground(true);
            }
        }

        const auto PostFxHandle = NewScene.Spawn("PostProcess");
        auto* PostFxActor       = NewScene.Get(PostFxHandle);
        if (PostFxActor) {
            auto* PostFx = PostFxActor->AddComponent<PostProcessComponent>();
            if (PostFx) {
                auto& Settings               = PostFx->GetSettings();
                Settings.BloomThreshold      = 0.8f;
                Settings.BloomIntensity      = 0.075f;
                Settings.BloomEnabled        = true;
                Settings.AutoExposureEnabled = true;
            }
        }

        SceneSerializer::SaveToFile(NewScene, SceneFile);
        if (exists(SceneFile)) { LoadSceneFile(SceneFile); }
    }

    void Editor::SetWindowTitle(const std::string& Title) const {
        const auto TitleFmt = std::format("XED - {} [{}]", Title, XEN_ENGINE_VERSION);
        _Window->SetTitle(TitleFmt);
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

    void Editor::EnsureDefaultLayout(const unsigned int DockspaceID, const f32 Width, const f32 Height) {
        if (ImGui::DockBuilderGetNode(DockspaceID)) return;  // a saved layout already exists

        ImGui::DockBuilderRemoveNode(DockspaceID);
        ImGui::DockBuilderAddNode(DockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(DockspaceID, ImVec2(Width, Height));

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
        const auto ThemePath = fs::current_path() / "Config" / "Themes" / ThemeFile;
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
        Style.ChildRounding    = _CurrentTheme.WindowRounding;
        Style.WindowBorderSize = _CurrentTheme.WindowBorderSize;
        Style.FrameBorderSize  = _CurrentTheme.FrameBorderSize;

        Style.WindowPadding = ImVec2(6.0f, 6.0f);
        Style.FramePadding  = ImVec2(6.0f, 6.0f);

        Colors[ImGuiCol_BorderShadow]       = ImVec4(0.f, 0.f, 0.f, 0.f);
        Colors[ImGuiCol_Border]             = _CurrentTheme.Colors.Border.To<ImVec4>();
        Colors[ImGuiCol_ButtonActive]       = _CurrentTheme.Colors.ButtonPrimary.WithAlpha(0.67f).To<ImVec4>();
        Colors[ImGuiCol_ButtonHovered]      = _CurrentTheme.Colors.ButtonPrimary.WithAlpha(0.8f).To<ImVec4>();
        Colors[ImGuiCol_Button]             = _CurrentTheme.Colors.ButtonPrimary.To<ImVec4>();
        Colors[ImGuiCol_CheckMark]          = _CurrentTheme.Colors.TextPrimary.To<ImVec4>();
        Colors[ImGuiCol_CheckboxSelectedBg] = _CurrentTheme.Colors.WindowBackground.To<ImVec4>();
        Colors[ImGuiCol_ChildBg]            = _CurrentTheme.Colors.PanelBackground.To<ImVec4>();
        Colors[ImGuiCol_DockingPreview]     = _CurrentTheme.Colors.TextPrimary.To<ImVec4>();
        Colors[ImGuiCol_DragDropTarget]     = _CurrentTheme.Colors.TextPrimary.To<ImVec4>();
        Colors[ImGuiCol_FrameBgActive]      = _CurrentTheme.Colors.Input.WithAlpha(0.4f).To<ImVec4>();
        Colors[ImGuiCol_FrameBgHovered]     = _CurrentTheme.Colors.Input.WithAlpha(0.7f).To<ImVec4>();
        Colors[ImGuiCol_FrameBg]            = _CurrentTheme.Colors.Input.To<ImVec4>();
        Colors[ImGuiCol_HeaderActive] =
          _CurrentTheme.Colors.WindowBackground.Lightened(0.3f).To<ImVec4>();  // Selected item in listbox
        Colors[ImGuiCol_HeaderHovered]         = _CurrentTheme.Colors.WindowBackground.Lightened(0.2f).To<ImVec4>();
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
        Colors[ImGuiCol_Separator]             = _CurrentTheme.Colors.Border.To<ImVec4>();
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
                    Actor* A = S->Get(State.SelectedActor);
                    if (A) {
                        {
                            ScopedFont _(&_UI, "InterBold");
                            ImGui::Text("%s", A->GetName().c_str());
                        }

                        State.ActorEnabled = A->IsEnabled();
                        UI::Controls::CheckBox("Enabled", &State.ActorEnabled);
                        A->SetEnabled(State.ActorEnabled);

                        State.TransformPosition = A->GetWorldTransform().Position;
                        const auto RotationQuat = A->GetWorldTransform().Rotation;
                        const auto EulerAngles  = QuaternionToEuler(RotationQuat);
                        State.TransformRotation = {DirectX::XMConvertToDegrees(EulerAngles.x),
                                                   DirectX::XMConvertToDegrees(EulerAngles.y),
                                                   DirectX::XMConvertToDegrees(EulerAngles.z)};
                        State.TransformScale    = A->GetWorldTransform().Scale;

                        UI::Controls::DragFloatNColored("Position", &State.TransformPosition.x, 3, 0.01f);
                        UI::Controls::DragFloatNColored("Rotation", &State.TransformRotation.x, 3, 0.01f);
                        UI::Controls::DragFloatNColored("Scale", &State.TransformScale.x, 3, 0.01f);

                        A->SetPosition(State.TransformPosition);
                        // Rotating on X axis mostly works, the other two axes just snap back to zero.
                        const Float3 NewRotation = {
                          DirectX::XMConvertToRadians(State.TransformRotation.x),
                          DirectX::XMConvertToRadians(State.TransformRotation.y),
                          DirectX::XMConvertToRadians(State.TransformRotation.z),
                        };
                        A->SetRotation(EulerToQuaternion(NewRotation));
                        A->SetScale(State.TransformScale);

                        // ===========================================================

                        A->ForEachComponent([&](IComponent* C) {
                            // Component identity (pointer, not index - a
                            // component's slot doesn't move around the way
                            // an actor's does, but PushID by pointer costs
                            // nothing and avoids ever having to reason about
                            // it) scopes every widget ID below to this one
                            // component, so two components that both reflect
                            // a property named e.g. "Enabled" - or a
                            // component reflected twice on the same actor -
                            // don't collide in ImGui's ID stack.
                            ImGui::PushID(C);

                            bool ComponentEnabled = C->IsEnabled();
                            if (UI::Controls::CheckBox("##ComponentEnabled", &ComponentEnabled)) {
                                C->SetEnabled(ComponentEnabled);
                            }
                            ImGui::SameLine();

                            if (ImGui::CollapsingHeader(C->GetTypeName(), ImGuiTreeNodeFlags_DefaultOpen)) {
                                ImGui::Indent();
                                PropertyEditorReflector Reflector;
                                C->Reflect(Reflector);
                                ImGui::Unindent();
                            }

                            ImGui::PopID();
                        });

                        {
                            ScopedFont _(&_UI, "InterBold");
                            if (ImGui::Button("Add Component", ImVec2(-FLT_MIN, 32.f))) {
                                ImGui::OpenPopup("Add Component");
                            }
                        }
                    }
                }
            }
        }

        Modal_AddComponent();

        ImGui::End();
    }

    void Editor::View_Scene(const f32 DeltaTime) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

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

        ImGui::PopStyleVar(2);
    }

    void Editor::Action_NewActor(Scene* S, const std::string& Name) const {
        S->Spawn(Name);
    }

    void Editor::Action_NewProject() const {
        State.ShowNewProjectModal = true;
    }

    void Editor::View_Hierarchy() const {
        if (ImGui::Begin("Hierarchy")) {
            if (_EmbeddedGame && _EmbeddedGame->GetActiveScene()) {
                auto* S = _EmbeddedGame->GetActiveScene();
                if (S) {
                    ImGui::InputText("Name", State.NewActorName.data(), State.NewActorName.size());
                    const std::string Name(State.NewActorName.data());
                    ImGui::BeginDisabled(Name.empty());
                    if (ImGui::Button("Add Actor", ImVec2(-FLT_MIN, 32.f))) {
                        Action_NewActor(S, Name);
                        State.NewActorName.fill('\0');
                    }
                    ImGui::EndDisabled();

                    ImGui::Spacing();

                    State.SceneActors.clear();
                    S->ForEachActor([&](const Actor& A) { State.SceneActors.push_back({A.GetName(), A.GetHandle()}); });

                    if (ImGui::BeginListBox("##Actors", ImVec2(-FLT_MIN, -FLT_MIN))) {
                        for (auto i = 0; i < State.SceneActors.size(); i++) {
                            const ActorListEntry& Entry = State.SceneActors[i];
                            const bool IsSelected       = (State.SelectedActor == Entry.Handle);
                            if (ImGui::Selectable(Entry.Name.c_str(), IsSelected)) {
                                State.SelectedActor = Entry.Handle;
                            }

                            if (IsSelected) ImGui::SetItemDefaultFocus();

                            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { State.SelectedActor = Entry.Handle; }

                            if (ImGui::BeginPopupContextItem(("##ActorContext" + std::to_string(i)).c_str())) {
                                if (ImGui::MenuItem("Duplicate")) { Action_DuplicateActor(S); }
                                ImGui::Separator();
                                if (ImGui::MenuItem("Delete")) { Action_DeleteActor(S); }
                                ImGui::EndPopup();
                            }
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
        if (ImGui::Begin("Log")) {
            Logger& Log = GetLogger();

            std::vector<Logger::Entry> Snapshot;
            {
                std::lock_guard Lock(Log.GetBufferMutex());
                const auto& Entries = Log.GetEntries();
                const size_t Total  = Log.GetTotalEntries();
                // Oldest entry is at index 0 until the ring has wrapped, after which the next write slot is the oldest.
                const size_t Start = Total < Logger::LOGGER_MAX_ENTRIES ? 0 : Log.GetCurrentEntryIndex();
                Snapshot.reserve(Total);
                for (size_t I = 0; I < Total; ++I) {
                    Snapshot.push_back(Entries[(Start + I) % Logger::LOGGER_MAX_ENTRIES]);
                }
            }

            if (ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
                // Only follow new lines if the user was already at the bottom.
                const bool WasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;

                for (const Logger::Entry& Entry : Snapshot) {
                    ImVec4 Color;
                    switch (Entry.Severity) {
                        case Logger::Severity::Warning:
                            Color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                            break;
                        case Logger::Severity::Error:
                        case Logger::Severity::Critical:
                            Color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
                            break;
                        case Logger::Severity::Debug:
                            Color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
                            break;
                        default:
                            Color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                            break;
                    }
                    ImGui::TextColored(Color, "[%s] %s", Entry.TimeStamp.c_str(), Entry.Message.c_str());
                }

                if (WasAtBottom) ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    void Editor::Action_OpenProject() {
        FileDialogs::FileTypeFilter Filter {
          .Name       = L"XED Project",
          .Extensions = L"*.prxj",
        };
        const auto SelectedResult = FileDialogs::OpenFileDialog(_Window->GetHandle(), L"Open XED project", {Filter});
        if (SelectedResult.has_value() && exists(*SelectedResult)) { LoadProject(*SelectedResult); }
    }

    void Editor::Action_OpenScene() const {
        if (!_EmbeddedGame || !_EmbeddedGame->IsRunning()) { return; }

        const auto OpenSceneResult = FileDialogs::OpenFileDialog(_Window->GetHandle(),
                                                                 L"XED - Open scene",
                                                                 {{L"XED Scene", L"*.xscene"}},
                                                                 _CurrentProject.ContentDirectory);
        if (OpenSceneResult.has_value()) { LoadSceneFile(*OpenSceneResult); }
    }

    void Editor::Action_ShowSettings() {
        _SettingsModal.Open();
    }

    void Editor::Action_NewScene() const {
        State.ShowNewSceneModal = true;
    }

    void Editor::Action_Quit() {
        _Running = false;
    }

    void Editor::Action_SaveScene() const {
        if (!_EmbeddedGame || !_EmbeddedGame->IsRunning() || !exists(State.CurrentSceneFile)) { return; }
        const auto* CurrentScene = _EmbeddedGame->GetActiveScene();
        if (!CurrentScene) { return; }

        SceneSerializer::SaveToFile(*CurrentScene, State.CurrentSceneFile);
        ::MessageBoxA(_Window->GetHandle(),
                      std::format("Saved {}", canonical(State.CurrentSceneFile).filename().string()).c_str(),
                      "XED",
                      MB_OK | MB_ICONINFORMATION);
    }

    void Editor::Action_SaveSceneAs() {}

    void Editor::Action_DeleteActor(Scene* S) const {
        if (!S) return;
        S->Destroy(State.SelectedActor);
        State.SelectedActor = ActorHandle::Invalid();
    }

    void Editor::Action_DuplicateActor(Scene* S) const {
        if (!S) return;
        const ActorHandle NewHandle = S->Clone(State.SelectedActor);
        // Select the new clone, same as most editors' own Duplicate - also
        // makes it immediately obvious the clone worked, rather than leaving
        // the original selected and the clone sitting unselected at the end
        // of the list.
        if (NewHandle.IsSet()) { State.SelectedActor = NewHandle; }
    }

    void Editor::Modal_AddComponent() const {
        UI::CenterNextWindow();

        if (ImGui::BeginPopupModal("Add Component", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Select which component to add");
            ImGui::Separator();

            if (ImGui::Button("Add", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }

            ImGui::EndPopup();
        }
    }

    void Editor::Modal_NewProject() {
        UI::CenterNextWindow();

        static std::array<char, MAX_PATH> ProjectName {};
        static std::array<char, MAX_PATH> ProjectDir {};

        if (ImGui::BeginPopupModal("New Project", nullptr)) {
            const std::string ProjectNameStr = ProjectName.data();
            const std::string ProjectDirStr  = ProjectDir.data();

            ImGui::InputText("Name", ProjectName.data(), ProjectName.size());

            ImGui::BeginDisabled(true);
            ImGui::InputText("##Location", ProjectDir.data(), ProjectDir.size());
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(ProjectNameStr.empty());
            if (ImGui::Button("...")) {
                const auto ProjectLocationResult =
                  FileDialogs::OpenFolderDialog(_Window->GetHandle(), L"Select project directory");
                if (ProjectLocationResult.has_value()) {
                    const auto Path = *ProjectLocationResult / ProjectNameStr;
                    strcpy_s(ProjectDir.data(), ProjectDir.size(), Path.string().c_str());
                }
            }
            ImGui::EndDisabled();

            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ProjectName.fill(0);
                ProjectDir.fill(0);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(ProjectNameStr.empty() || ProjectDirStr.empty());
            if (ImGui::Button("Create", ImVec2(120, 0))) {
                const auto Result   = CreateProject(ProjectNameStr, ProjectDirStr);
                bool ProjectCreated = false;

                if (Result == CreateProjectResult::Success) {
                    ProjectCreated = true;
                } else if (Result == CreateProjectResult::AlreadyExists) {
                    const auto MsgResult = ::MessageBoxA(_Window->GetHandle(),
                                                         "The selected project directory already exists. Do you want "
                                                         "to overwrite it and create a new project anyways?",
                                                         "XED - Create project",
                                                         MB_YESNO | MB_ICONWARNING);
                    if (MsgResult == IDYES) {
                        if (!fs::remove_all(ProjectDirStr)) {
                            ::MessageBoxA(_Window->GetHandle(),
                                          "Failed to remove existing directory.",
                                          "XED - Create project",
                                          MB_OK | MB_ICONERROR);
                        } else {
                            const auto TryAgainResult = CreateProject(ProjectNameStr, ProjectDirStr);
                            if (TryAgainResult != CreateProjectResult::Success) {
                                ::MessageBoxA(_Window->GetHandle(),
                                              "Failed to create new project.",
                                              "XED - Create project",
                                              MB_OK | MB_ICONERROR);
                            } else {
                                ProjectCreated = true;
                            }
                        }
                    }
                } else if (Result == CreateProjectResult::Failed) {
                    ::MessageBoxA(_Window->GetHandle(),
                                  "Failed to create new project.",
                                  "XED - Create project",
                                  MB_OK | MB_ICONERROR);
                }

                if (ProjectCreated) {
                    const auto MsgFmt =
                      std::format("Successfully created new project '{}'. Open it now?", ProjectNameStr);
                    const auto MsgResult = ::MessageBoxA(_Window->GetHandle(),
                                                         MsgFmt.c_str(),
                                                         "XED - Create project",
                                                         MB_YESNO | MB_ICONQUESTION);
                    if (MsgResult == IDYES) {
                        const auto PrxjPath =
                          (fs::path(ProjectDirStr) / fs::path(ProjectNameStr)).replace_extension(".prxj");
                        LoadProject(PrxjPath);
                    }
                }

                ProjectName.fill(0);
                ProjectDir.fill(0);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::SetItemDefaultFocus();

            ImGui::EndPopup();
        }
    }

    void Editor::Modal_NewScene() const {
        UI::CenterNextWindow();

        static std::array<char, MAX_PATH> SceneNameBuffer {};

        if (ImGui::BeginPopupModal("New Scene", nullptr)) {
            const std::string SceneName = SceneNameBuffer.data();
            ImGui::InputText("Scene Name", SceneNameBuffer.data(), SceneNameBuffer.size());

            ImGui::BeginDisabled(SceneName.empty());
            if (ImGui::Button("Create", ImVec2(120, 0))) {
                const auto ScenesDir = _CurrentProject.ContentDirectory / "scenes";
                if (!exists(ScenesDir)) { fs::create_directory(ScenesDir); }

                std::string SceneFileName = SceneName;
                std::ranges::transform(SceneFileName, SceneFileName.begin(), ::tolower);
                const auto SceneFilePath = (ScenesDir / SceneFileName).replace_extension(".xscene");

                CreateScene(SceneName, SceneFilePath);

                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();

            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }

            ImGui::EndPopup();
        }
    }

    void Editor::RegisterShortcut(const int Keys, std::function<void()> Action) {
        _Shortcuts.push_back({
          .Keys   = Keys,
          .Action = std::move(Action),
        });
    }

    void Editor::ProcessShortcuts() const {
        for (const auto& [Keys, Action] : _Shortcuts) {
            if (ImGui::Shortcut(CAST<ImGuiKeyChord>(Keys), ImGuiInputFlags_RouteGlobal)) { Action(); }
        }
    }

    void Editor::SetupShortcuts() {
        RegisterShortcut(ImGuiMod_Ctrl | ImGuiKey_O, [this] { Action_OpenProject(); });
        RegisterShortcut(ImGuiMod_Ctrl | ImGuiKey_Q, [this] { Action_Quit(); });
        RegisterShortcut(ImGuiMod_Ctrl | ImGuiKey_S, [this] { Action_SaveScene(); });
        RegisterShortcut(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_S, [this] { Action_SaveSceneAs(); });
        RegisterShortcut(ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_S, [this] { Action_ShowSettings(); });
        RegisterShortcut(ImGuiMod_Ctrl | ImGuiKey_N, [this] { Action_NewProject(); });
        RegisterShortcut(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_O, [this] { Action_OpenScene(); });
        RegisterShortcut(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_N, [this] { Action_NewScene(); });
    }

    void Editor::DrawMainMenuBar() {
        // TODO: Add associated functions for creating the component. Possible some kind of factory?
        static const std::vector<std::string> AddComponentMenuItems = {
          "Ambient Occlusion",
          "Anti Aliasing",
          "Audio Source",
          "Camera",
          "Directional Light",
          "Environment",
          "First Person Player Controller",
          "Mesh",
          "Material",
          "Point Light",
          "Post Process",
          "Spot Light",
          "Sprite",
        };

        const bool SceneReadyToEdit =
          _EmbeddedGame != nullptr && _EmbeddedGame->IsRunning() && _EmbeddedGame->GetActiveScene() != nullptr;

        if (!ImGui::BeginMainMenuBar()) return;

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project", "Ctrl+N", false, true)) { Action_NewProject(); }
            if (ImGui::MenuItem("Open Project", "Ctrl+O", false, true)) { Action_OpenProject(); }

            ImGui::Separator();

            const bool SceneOptionsEnabled = _EmbeddedGame != nullptr && _EmbeddedGame->IsRunning();
            if (ImGui::MenuItem("New Scene", "Ctrl+Shift+N", false, SceneOptionsEnabled)) { Action_NewScene(); }
            if (ImGui::MenuItem("Open Scene", "Ctrl+Shift+O", false, SceneOptionsEnabled)) { Action_OpenScene(); }

            const bool SaveOptionsEnabled =
              _EmbeddedGame != nullptr && _EmbeddedGame->GetActiveScene() != nullptr && exists(State.CurrentSceneFile);
            if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, SaveOptionsEnabled)) { Action_SaveScene(); }
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S", false, SaveOptionsEnabled)) {
                Action_SaveSceneAs();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Exit", "Ctrl+Q")) { Action_Quit(); }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
            ImGui::MenuItem("Redo", "Ctrl+Y", false, false);
            ImGui::Separator();
            ImGui::MenuItem("Cut", "Ctrl+X", false, false);
            ImGui::MenuItem("Copy", "Ctrl+C", false, false);
            ImGui::MenuItem("Paste", "Ctrl+V", false, false);
            ImGui::Separator();

            if (ImGui::MenuItem("Settings...", "Ctrl+Alt+S", false, true)) { Action_ShowSettings(); }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Add", SceneReadyToEdit)) {
            if (ImGui::BeginMenu("Actor")) {
                if (ImGui::MenuItem("Empty", nullptr, false, true)) {}

                if (ImGui::MenuItem("Sun", nullptr, false, true)) {}

                if (ImGui::MenuItem("Camera", nullptr, false, true)) {}

                if (ImGui::MenuItem("Environment", nullptr, false, true)) {}

                ImGui::EndMenu();
            }

            const bool AddComponentEnabled =
              SceneReadyToEdit && _EmbeddedGame->GetActiveScene()->Get(State.SelectedActor) != nullptr;
            if (ImGui::BeginMenu("Component", AddComponentEnabled)) {
                for (auto& Item : AddComponentMenuItems) {
                    if (ImGui::MenuItem(Item.c_str(), nullptr, false, true)) {}
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Build", SceneReadyToEdit)) {
            ImGui::MenuItem("Build Project", nullptr, false, false);
            ImGui::MenuItem("Rebuild", nullptr, false, false);
            ImGui::MenuItem("Clean", nullptr, false, false);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Reset Layout", nullptr, false, false);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("Documentation", nullptr, false, false);
            ImGui::MenuItem("About XED", nullptr, false, false);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();

        if (State.ShowNewProjectModal) {
            ImGui::OpenPopup("New Project");
            State.ShowNewProjectModal = false;
        }

        if (State.ShowNewSceneModal) {
            ImGui::OpenPopup("New Scene");
            State.ShowNewSceneModal = false;
        }

        _SettingsModal.Draw();
        Modal_NewProject();
        Modal_NewScene();
    }

    void Editor::DrawToolbar() {
        const ImGuiViewport* Viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(Viewport->WorkPos);
        ImGui::SetNextWindowSize(ImVec2(Viewport->WorkSize.x, ToolbarHeight));
        ImGui::SetNextWindowViewport(Viewport->ID);

        constexpr ImGuiWindowFlags ToolbarFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                                  ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
                                                  ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        if (ImGui::Begin("##Toolbar", nullptr, ToolbarFlags)) {
            constexpr ImVec2 IconSize(20.0f, 20.0f);
            const f32 ButtonHeight = IconSize.y + ImGui::GetStyle().FramePadding.y * 2.0f;
            ImGui::SetCursorPosY((ToolbarHeight - ButtonHeight) * 0.5f);

            const auto ToolbarButton = [this, IconSize](const char* StrID, const EditorIcon Icon, const bool Enabled) {
                const ImTextureID TexID = _Icons.Get(Icon);

                bool Clicked = false;
                ImGui::BeginDisabled(!Enabled);
                if (TexID != 0) {
                    Clicked = ImGui::ImageButton(StrID, TexID, IconSize);
                } else {
                    ImGui::Dummy(IconSize);
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                return Clicked;
            };

            if (ToolbarButton("##OpenFolder", EditorIcon::OpenFolder, true)) { Action_OpenProject(); }

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 2.0f);
            ToolbarButton("##Undo", EditorIcon::Undo, false);
            ToolbarButton("##Redo", EditorIcon::Redo, false);

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 2.0f);
            ToolbarButton("##Select", EditorIcon::Select, false);
            ToolbarButton("##Move", EditorIcon::Move, false);
            ToolbarButton("##Rotate", EditorIcon::Rotate, false);
            ToolbarButton("##Scale", EditorIcon::Scale, false);
            ToolbarButton("##FocusSelected", EditorIcon::FocusSelected, false);
            ToolbarButton("##SelectAsset", EditorIcon::SelectAsset, false);
            ToolbarButton("##GridToggle", EditorIcon::GridToggle, false);

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 2.0f);
            ToolbarButton("##Play", EditorIcon::Play, false);
            ToolbarButton("##PlayWindowed", EditorIcon::PlayWindowed, false);
            ToolbarButton("##Pause", EditorIcon::Pause, false);
            ToolbarButton("##Stop", EditorIcon::Stop, false);

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 2.0f);
            ToolbarButton("##CompileCode", EditorIcon::CompileCode, false);
            ToolbarButton("##CleanCode", EditorIcon::CleanCode, false);
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void Editor::DrawDockspaceAndPanels(const f32 DeltaTime) {
        ProcessShortcuts();

        DrawMainMenuBar();
        DrawToolbar();

        const ImGuiViewport* Viewport = ImGui::GetMainViewport();
        const ImVec2 DockspacePos(Viewport->WorkPos.x, Viewport->WorkPos.y + ToolbarHeight);
        const ImVec2 DockspaceSize(Viewport->WorkSize.x, Viewport->WorkSize.y - ToolbarHeight);

        ImGui::SetNextWindowPos(DockspacePos);
        ImGui::SetNextWindowSize(DockspaceSize);
        ImGui::SetNextWindowViewport(Viewport->ID);

        constexpr ImGuiWindowFlags HostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                               ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                               ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("EditorDockspaceHost", nullptr, HostFlags);
        ImGui::PopStyleVar(3);

        const ImGuiID DockspaceID = ImGui::GetID("EditorDockspace");
        EnsureDefaultLayout(DockspaceID, DockspaceSize.x, DockspaceSize.y);
        ImGui::DockSpace(DockspaceID, ImVec2(0.0f, 0.0f));
        ImGui::End();

        View_Scene(DeltaTime);
        View_Hierarchy();
        View_Inspector();
        View_ContentBrowser();
        View_Log();
    }
}  // namespace Xen
