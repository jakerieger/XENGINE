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
#include <Xen/ComponentRegistry.hpp>

#pragma region Components
#include <Xen/Components/AmbientOcclusionComponent.hpp>
#include <Xen/Components/AntiAliasingComponent.hpp>
#include <Xen/Components/AudioSourceComponent.hpp>
#include <Xen/Components/CameraComponent.hpp>
#include <Xen/Components/CharacterControllerComponent.hpp>
#include <Xen/Components/DirectionalLightComponent.hpp>
#include <Xen/Components/EnvironmentComponent.hpp>
#include <Xen/Components/FPPlayerController.hpp>
#include <Xen/Components/MeshComponent.hpp>
#include <Xen/Components/PointLightComponent.hpp>
#include <Xen/Components/PostProcessComponent.hpp>
#include <Xen/Components/PrefabSpawnerComponent.hpp>
#include <Xen/Components/RigidBodyComponent.hpp>
#include <Xen/Components/SpotLightComponent.hpp>
#include <Xen/Components/SpriteComponent.hpp>
#pragma endregion

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <memory>
#include <Lmcons.h>  // contains UNLEN (maximum length of Windows username)

#pragma region Embedded Resources
#include <Xen/MaterialBindings.hpp>
#include "Resource/InterRegular.h"
#include "Resource/InterBold.h"
#include "XenPAK/Canonicalize.hpp"
#pragma endregion

namespace Xen {

    namespace {
        enum class PlayState : u8 { Edit, Playing, Paused };

        struct ActorListEntry {
            std::string Name;
            ActorHandle Handle;
        };

        struct EditorState {
            std::vector<ActorListEntry> SceneActors;
            ActorHandle SelectedActor {};

            bool ActorEnabled {false};
            Float3 TransformPosition {};
            Float3 TransformRotation {};
            Float3 TransformScale {};
            std::array<char, MAX_PATH> NewActorName {'\0'};

            // Modal flags
            bool ShowNewProjectModal {false};
            bool ShowNewSceneModal {false};
            bool ShowNewComponentClassModal {false};

            // "Save as Prefab": the actor being saved, and the name typed for the file.
            bool ShowSavePrefabModal {false};
            ActorHandle PrefabSourceActor {};
            std::array<char, MAX_PATH> PrefabName {'\0'};
            std::string PrefabError;

            fs::path CurrentSceneFile {};

            // Type name picked in the Add Component dialog.
            std::string AddComponentChoice;

            // Edit mode vs play mode. The Game's simulation (OnUpdate, component
            // Tick) only runs while Playing; entering play snapshots the scene
            // and Stop restores it, so nothing a play session does sticks.
            PlayState Play {PlayState::Edit};
            Json PlaySnapshot;
            u64 PlaySelectedActorID {0};
            // The scene Play started in - Stop restores into it only if it's
            // still the active one (game code may have switched scenes).
            const Scene* PlayScene {nullptr};

            // Hot reload: the edited scene, waiting for the rebuilt game to
            // have a live Scene to load it into (see PollPendingRestore).
            bool PendingRestore {false};
            Json RestoreSnapshot;
            u64 RestoreSelectedActorID {0};
        };

        // Every registered component type - the engine's plus the loaded game
        // module's - by name, sorted for display.
        std::vector<std::string> GetAddableComponentNames() {
            auto Names = ComponentRegistry::Get().GetRegisteredNames();
            std::ranges::sort(Names);
            return Names;
        }

        void AddComponentByName(Actor* A, const std::string& TypeName) {
            if (!A) return;
            if (auto Created = ComponentRegistry::Get().Create(TypeName)) {
                A->AdoptComponent(std::move(Created));
            } else {
                LOG_ERR("Can't add component '%s' - not registered", TypeName.c_str());
            }
        }

        constexpr f32 ToolbarHeight = 40.0f;

        // The engine install this XED belongs to, for pointing new projects'
        // CMake at it. An installed XED lives at <install root>/XED/Bin64,
        // and FixContentWorkingDirectory makes <install root>/XED the working
        // directory. Empty when XED runs from a build tree instead of an
        // install - there's no find_package(Xen) package to point at there.
        fs::path FindEngineInstallRoot() {
            const fs::path Root = fs::current_path().parent_path();
            if (exists(Root / "share" / "Xen" / "cmake" / "XenConfig.cmake")) return Root;
            return {};
        }
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

        _EditorSettings = EditorSettings::Load("Config/EditorSettings.ini");
        if (_EditorSettings.StartupMode == EditorStartupMode::Maximized) _Window->Maximize();
        if (!_EditorSettings.UITheme.empty()) LoadTheme(_EditorSettings.UITheme);

        // We'll check that it exists here even though LoadProject already checks to avoid throwing an exception if it
        // doesn't. The editor should still start if the startup project is invalid and just prompt the user to select
        // or create a new project to load. Later, a flag of some kind will be added that tells the editor this failed.
        if (!_EditorSettings.StartupProject.empty() && fs::exists(_EditorSettings.StartupProject)) {
            LoadProject(_EditorSettings.StartupProject);
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

            // Ends the window's input frame (edge detection, mouse deltas) -
            // an embedded Game never does this itself, having no Window.
            _Window->ResetInput();
            _Window->PollEvents();
        }
    }

    void Editor::LoadProject(const fs::path& PrxjPath) {
        if (!exists(PrxjPath)) {
            LOG_ERR("Project does not exist: '%s'", PrxjPath.string().c_str());
            _EditorSettings.StartupProject.clear();
            std::ignore = _EditorSettings.Save();
            return;
        }

        const auto LoadResult = ProjectSerializer::LoadFromFile(PrxjPath);
        if (!LoadResult.has_value()) {
            THROW_ENGINE_EXCEPTION(EditorException, "Failed to load project (error during parsing)");
        }

        // Tear down whatever project was open first: its Game must die before
        // its module unloads, and the selection points into its scene.
        UnloadProject();

        _CurrentProject = *LoadResult;
        if (_CurrentProject.ProjectRoot.empty()) { _CurrentProject.ProjectRoot = PrxjPath.parent_path(); }

        // Content browser starts at the project's Content directory.
        _ContentBrowser.SetRoot(_CurrentProject.ContentDirectory);
        _AssetIndex.Rescan(_CurrentProject.ContentDirectory);

        StartEmbeddedGame();

        SetWindowTitle(_CurrentProject.Name);
        _EditorSettings.StartupProject = PrxjPath;
        if (!_EditorSettings.Save()) { LOG_ERR("Failed to save EditorSettings.ini"); }

        const auto& EngineConfig = _EmbeddedGame->_EngineConfig;
        if (!EngineConfig.StartupScene.empty()) {
            const auto SceneFilePath = _CurrentProject.ContentDirectory / EngineConfig.StartupScene;
            LoadSceneFile(SceneFilePath);
        }
    }

    void Editor::StartEmbeddedGame() {
        PAK::AssetMountConfig MountConfig = BuildMountConfig(Xen::Generated::GameSettings(), 0, nullptr);
        MountConfig.ContentDirs           = {_CurrentProject.ContentDirectory};

        const bool HasModule = LoadGameModule();
        if (HasModule) {
            const EmbeddedGameArgs Args {
              .Device      = _Device.get(),
              .MountConfig = &MountConfig,
              .Width       = _SceneViewportWidth,
              .Height      = _SceneViewportHeight,
              .ConfigRoot  = &_CurrentProject.ConfigDirectory,
            };
            _EmbeddedGame = {_GameModule.CreateGame(Args), GameDeleter {&_GameModule}};
        } else {
            _EmbeddedGame = {new Game(*_Device,
                                      MountConfig,
                                      _SceneViewportWidth,
                                      _SceneViewportHeight,
                                      _CurrentProject.ConfigDirectory),
                             GameDeleter {}};
        }
        // Edit mode: the scene renders but the game doesn't run until Play.
        _EmbeddedGame->SetSimulationEnabled(false);
        _EmbeddedGame->StartEmbedded();
    }

    void Editor::PollBuild() {
        std::vector<std::string> Lines;
        int ExitCode        = 0;
        const bool Finished = _Build.Poll(Lines, ExitCode);

        for (const auto& Line : Lines) {
            if (Line.empty()) continue;
            if (Line.find("error") != std::string::npos || Line.find("FAILED") != std::string::npos) {
                LOG_ERR("%s", Line.c_str());
            } else if (Line.find("warning") != std::string::npos) {
                LOG_WARN("%s", Line.c_str());
            } else {
                LOG_INFO("%s", Line.c_str());
            }
        }

        if (!Finished) return;

        if (ExitCode != 0) {
            LOG_ERR("Build failed (exit code %d)%s",
                    ExitCode,
                    _BuildTarget == BuildTarget::Module ? " - keeping the loaded module" : "");
            return;
        }
        if (_BuildTarget != BuildTarget::Module) {
            // A game build (exe + packed content) - nothing for the editor to reload.
            LOG_INFO("%s build succeeded", _BuildTarget == BuildTarget::Debug ? "Debug" : "Release");
            return;
        }
        if (_BuildProjectRoot != _CurrentProject.ProjectRoot) {
            LOG_INFO("Compile finished, but a different project is open now - not reloading");
            return;
        }
        LOG_INFO("Compile succeeded - reloading the game module");
        ReloadGameModule();
    }

    void Editor::ReloadGameModule() {
        if (_CurrentProject.Name.empty()) return;

        // Leave play mode first, restoring the scene as it was - the snapshot
        // below must be the edited scene, not a play-time state.
        Action_Stop();

        Json Snapshot;
        u64 SelectedID    = 0;
        bool HaveSnapshot = false;
        if (const Scene* Active = _EmbeddedGame ? _EmbeddedGame->GetActiveScene() : nullptr) {
            try {
                Snapshot = SceneSerializer::SaveToJson(*Active);
                if (const Actor* Selected = Active->Get(State.SelectedActor)) { SelectedID = Selected->GetActorID(); }
                HaveSnapshot = true;
            } catch (const std::exception& Ex) {
                LOG_ERR("Reload aborted - failed to snapshot the scene: %s", Ex.what());
                return;
            }
        }
        const fs::path SceneFile = State.CurrentSceneFile;

        // Nothing GPU-side may still reference the old game's resources, and
        // nothing may still point into its components: UnloadProject clears
        // the selection and destroys the Game (and so every component, whose
        // code lives in the module) before the DLL is unloaded.
        _Device->WaitIdle();
        UnloadProject();
        StartEmbeddedGame();

        // The new Game starts empty. Load a scene to get a live Scene object,
        // then PollPendingRestore overwrites its contents with the snapshot.
        fs::path SceneToLoad = SceneFile;
        if (!exists(SceneToLoad)) {
            const auto& Startup = _EmbeddedGame->_EngineConfig.StartupScene;
            SceneToLoad         = Startup.empty() ? fs::path {} : _CurrentProject.ContentDirectory / Startup;
        }
        if (HaveSnapshot && exists(SceneToLoad)) {
            _EmbeddedGame->LoadSceneFromFile(SceneToLoad);
            State.CurrentSceneFile       = SceneFile;
            State.PendingRestore         = true;
            State.RestoreSnapshot        = std::move(Snapshot);
            State.RestoreSelectedActorID = SelectedID;
        } else if (exists(SceneToLoad)) {
            LoadSceneFile(SceneToLoad);
        }
    }

    void Editor::PollPendingRestore() {
        if (!State.PendingRestore || !_EmbeddedGame) return;
        Scene* Active = _EmbeddedGame->GetActiveScene();
        if (!Active) return;  // still loading

        State.PendingRestore = false;
        try {
            SceneSerializer::LoadFromJson(*Active, State.RestoreSnapshot);
            if (State.RestoreSelectedActorID != 0) {
                Active->ForEachActor([](const Actor& A) {
                    if (A.GetActorID() == State.RestoreSelectedActorID) { State.SelectedActor = A.GetHandle(); }
                });
            }
            LOG_INFO("Scene restored after reload");
        } catch (const std::exception& Ex) { LOG_ERR("Failed to restore the scene after reload: %s", Ex.what()); }
        State.RestoreSnapshot = Json();

        if (exists(State.CurrentSceneFile)) {
            SetWindowTitle(
              std::format("{} ({})", canonical(State.CurrentSceneFile).filename().string(), _CurrentProject.Name));
        }
    }

    void Editor::UnloadProject() {
        SetGameHasInput(false);
        State.PendingRestore  = false;
        State.RestoreSnapshot = Json();
        State.SelectedActor   = ActorHandle::Invalid();
        State.SceneActors.clear();
        State.CurrentSceneFile.clear();
        State.Play         = PlayState::Edit;
        State.PlaySnapshot = Json();
        State.PlayScene    = nullptr;

        _EmbeddedGame.reset();
        _GameModule.Unload();
    }

    bool Editor::LoadGameModule() {
        const fs::path BuildDir = _CurrentProject.ModuleDirectory.empty() ? _CurrentProject.ProjectRoot / "build"
                                                                          : _CurrentProject.ModuleDirectory;
        const auto ModuleName   = _CurrentProject.Name + "Module.dll";

        std::error_code Ec;
        if (!fs::is_directory(BuildDir, Ec)) {
            LOG_INFO("No build directory for '%s' - using the base Game (build the 'editor' preset for game code)",
                     _CurrentProject.Name.c_str());
            return false;
        }

        fs::path Newest;
        fs::file_time_type NewestTime {};
        for (fs::recursive_directory_iterator It(BuildDir, fs::directory_options::skip_permission_denied, Ec), End;
             !Ec && It != End;
             It.increment(Ec)) {
            if (!It->is_regular_file(Ec) || It->path().filename() != ModuleName) continue;

            if (const auto Time = It->last_write_time(Ec); Newest.empty() || Time > NewestTime) {
                Newest     = It->path();
                NewestTime = Time;
            }
        }
        if (Newest.empty()) {
            LOG_INFO("No %s under '%s' - using the base Game (build the 'editor' preset for game code)",
                     ModuleName.c_str(),
                     BuildDir.string().c_str());
            return false;
        }

        // A fresh folder per load, so a rebuild never collides with the copy
        // still loaded from the previous one.
        static u32 LoadCounter = 0;
        if (LoadCounter == 0) {
            // Copies left over from earlier sessions. Best effort: a copy
            // still loaded by another running XED just fails to delete.
            fs::remove_all(fs::temp_directory_path() / "XED" / _CurrentProject.Name, Ec);
        }
        const fs::path CopyDir =
          fs::temp_directory_path() / "XED" / _CurrentProject.Name / std::to_string(++LoadCounter);
        fs::create_directories(CopyDir, Ec);

        const fs::path CopiedDll = CopyDir / Newest.filename();
        fs::copy_file(Newest, CopiedDll, fs::copy_options::overwrite_existing, Ec);
        if (Ec) {
            LOG_ERR("Failed to copy game module to '%s': %s", CopyDir.string().c_str(), Ec.message().c_str());
            return false;
        }

        // Keep the PDB next to the copy so a debugger attached to XED finds it.
        fs::path Pdb = Newest;
        Pdb.replace_extension(".pdb");
        if (fs::exists(Pdb, Ec)) {
            fs::copy_file(Pdb, CopyDir / Pdb.filename(), fs::copy_options::overwrite_existing, Ec);
        }

        std::string Error;
        if (!_GameModule.Load(CopiedDll, Error)) {
            LOG_ERR("%s - using the base Game", Error.c_str());
            return false;
        }
        return true;
    }

    Editor::CreateProjectResult Editor::CreateProject(const std::string& Name, const fs::path& Dir) {
        EditorProject Project;
        Project.Name             = Name;
        Project.ProjectRoot      = Dir;
        Project.Version          = XED_PROJECT_FORMAT_VERSION;
        Project.ConfigDirectory  = "Config";
        Project.ContentDirectory = "Content";
        Project.RuntimeDirectory = "Runtime";

        if (exists(Dir)) { return CreateProjectResult::AlreadyExists; }
        if (!fs::create_directories(Dir)) {
            LOG_ERR("Failed to create project directory: '%s'", Dir.string().c_str());
            return CreateProjectResult::Failed;
        }

        // XenBuild.bat - build tool
        if (!fs::copy_file("Templates/XenBuild.bat", Dir / "XenBuild.bat")) {
            LOG_ERR("Failed to copy XenBuild.bat to: '%s'", (Dir / "XenBuild.bat").string().c_str());
            return CreateProjectResult::Failed;
        }

        const auto ConfigDir = Dir / "Config";
        if (!fs::create_directories(ConfigDir)) { return CreateProjectResult::Failed; }

        // Copy config templates to new project config dir
        if (!fs::copy_file("Templates/Config/AudioConfig.ini", ConfigDir / "AudioConfig.ini")) {
            LOG_ERR("Failed to create Config/AudioConfig.ini");
            return CreateProjectResult::Failed;
        }
        if (!fs::copy_file("Templates/Config/EngineConfig.ini", ConfigDir / "EngineConfig.ini")) {
            LOG_ERR("Failed to create Config/EngineConfig.ini");
            return CreateProjectResult::Failed;
        }
        if (!fs::copy_file("Templates/Config/InputConfig.ini", ConfigDir / "InputConfig.ini")) {
            LOG_ERR("Failed to create Config/InputConfig.ini");
            return CreateProjectResult::Failed;
        }

        const auto ContentDir = Dir / "Content";
        if (!fs::create_directories(ContentDir)) {
            LOG_ERR("Failed to create Content directory");
            return CreateProjectResult::Failed;
        }

        // Starter scene (camera, light, sky, post-processing) - the template
        // EngineConfig.ini's StartupScene points here. Without an active
        // scene a Game renders nothing at all, not even OnRender/DebugUI.
        if (!fs::create_directories(ContentDir / "scenes")) { return CreateProjectResult::Failed; }
        if (!fs::copy_file("Templates/Content/scenes/main.xscene", ContentDir / "scenes" / "main.xscene")) {
            LOG_ERR("Failed to create main.xscene");
            return CreateProjectResult::Failed;
        }

        const auto RuntimeDir = Dir / "Runtime";
        if (!fs::create_directories(RuntimeDir)) {
            LOG_ERR("Failed to create Runtime directory");
            return CreateProjectResult::Failed;
        }

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

            const fs::path EngineRoot = FindEngineInstallRoot();

            std::unordered_map<std::string, std::string> TemplateVars = {
              {"GAME_CLASS", Name},
              {"USER", Username},
              {"DATE", DateTime::Now().DateString()},
              {"ENGINE_VERSION", XEN_ENGINE_VERSION},
              // Forward slashes - it ends up in JSON/CMake, where '\' escapes.
              {"XEN_INSTALL_ROOT", EngineRoot.generic_string()},
            };

            auto CMakeListsTemplate  = IO::ReadString("Templates/CMakeLists.txt");
            const auto CMakeListsTxt = ParseTemplate(CMakeListsTemplate, TemplateVars);
            IO::WriteString(CMakeListsTxt, CMakeListsTxtPath);

            // Machine-specific (an absolute path to this engine install), hence
            // CMakeUserPresets.json rather than the shared CMakePresets.json.
            if (!EngineRoot.empty()) {
                auto PresetsTemplate = IO::ReadString("Templates/CMakeUserPresets.json");
                IO::WriteString(ParseTemplate(PresetsTemplate, TemplateVars), Dir / "CMakeUserPresets.json");
            } else {
                LOG_WARN("XED isn't running from an engine install - the new project's CMake needs "
                         "-DCMAKE_PREFIX_PATH=<engine install dir> to find the engine");
            }

            auto MainCppTemplate = IO::ReadString("Templates/Runtime/main.cpp");
            const auto MainCpp   = ParseTemplate(MainCppTemplate, TemplateVars);
            IO::WriteString(MainCpp, MainCppPath);

            auto GameClassCppTemplate = IO::ReadString("Templates/Runtime/GameClass.cpp");
            const auto GameClassCpp   = ParseTemplate(GameClassCppTemplate, TemplateVars);
            IO::WriteString(GameClassCpp, GameClassCppPath);

            auto GameClassHppTemplate = IO::ReadString("Templates/Runtime/GameClass.hpp");
            const auto GameClassHpp   = ParseTemplate(GameClassHppTemplate, TemplateVars);
            IO::WriteString(GameClassHpp, GameClassHppPath);
        } catch (const std::exception& Ex) {
            LOG_ERR("Failed to create runtime sources: %s", Ex.what());
            return CreateProjectResult::Failed;
        } catch (...) {
            LOG_ERR("Failed to create runtime sources");
            return CreateProjectResult::Failed;
        }

        const auto PrxjPath = (Dir / Name).replace_extension(".prxj");
        try {
            ProjectSerializer::SaveToFile(Project, PrxjPath);
        } catch (...) {
            LOG_ERR("Failed to create .prxj project file");
            return CreateProjectResult::Failed;
        }

        return CreateProjectResult::Success;
    }

    void Editor::CreateComponentClass(const std::string& ClassName) {
        if (isdigit(ClassName[0])) {
            ::MessageBoxA(_Window->GetHandle(),
                          "Component class names must begin with a letter.",
                          "New Component Class",
                          MB_OK | MB_ICONERROR);
            return;
        }

        std::string Name = ClassName;
        std::erase_if(Name, [](const u8 C) { return std::isspace(C); });  // Remove any whitespace

        try {
            const auto HeaderTemplate = IO::ReadString("Templates/Runtime/Component.hpp");
            const auto SourceTemplate = IO::ReadString("Templates/Runtime/Component.cpp");

            char Username[UNLEN + 1];
            DWORD UsernameLen = UNLEN + 1;
            if (!::GetUserNameA(Username, &UsernameLen)) {
                LOG_ERR("Failed to get user name");
                strcpy_s(Username, UsernameLen, "Unknown");
            }

            std::unordered_map<std::string, std::string> TemplateVars = {
              {"COMPONENT_CLASS", Name},
              {"USER", Username},
              {"DATE", DateTime::Now().DateString()},
            };

            const auto HeaderPath = (_CurrentProject.RuntimeDirectory / Name).replace_extension(".hpp");
            const auto SourcePath = (_CurrentProject.RuntimeDirectory / Name).replace_extension(".cpp");

            const auto ParsedHeader = ParseTemplate(HeaderTemplate, TemplateVars);
            const auto ParsedSource = ParseTemplate(SourceTemplate, TemplateVars);

            IO::WriteString(ParsedHeader, HeaderPath);
            IO::WriteString(ParsedSource, SourcePath);

            // Update CMakeLists.txt
            const auto SourceSource        = std::format("Runtime/{}.cpp", Name);
            const auto HeaderSource        = std::format("Runtime/{}.hpp", Name);
            const std::vector CMakeSources = {SourceSource, HeaderSource};

            std::string Error;
            if (!CMakeSourceEditor::AddSourcesToFile(_CurrentProject.ProjectRoot / "CMakeLists.txt",
                                                     _CurrentProject.Name,
                                                     CMakeSources,
                                                     Error)) {
                const auto ErrFmt = std::format("CMakeLists.txt failed to add sources to file: {}", Error);
                LOG_ERR(ErrFmt.c_str());
                ::MessageBoxA(_Window->GetHandle(), ErrFmt.c_str(), "New Component Class", MB_OK | MB_ICONERROR);
                return;
            }

            const auto MsgFmt = std::format("Successfully created new component class: {}", Name);
            LOG_INFO(MsgFmt.c_str());
            ::MessageBoxA(_Window->GetHandle(), MsgFmt.c_str(), "New Component Class", MB_OK | MB_ICONINFORMATION);
        } catch (...) {
            ::MessageBoxA(_Window->GetHandle(),
                          "Failed to create new component class",
                          "New Component Class",
                          MB_OK | MB_ICONERROR);
        }
    }

    void Editor::LoadSceneFile(const fs::path& SceneFile) const {
        if (!_EmbeddedGame || !_EmbeddedGame->IsRunning()) { return; }

        // Loading replaces the scene a play session was running in - back to
        // edit mode, nothing to restore.
        _EmbeddedGame->SetSimulationEnabled(false);
        State.Play         = PlayState::Edit;
        State.PlaySnapshot = Json();
        State.PlayScene    = nullptr;

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
        // Before BeginFrame: a reload destroys and recreates the Game, which
        // must not happen inside the frame bracket below.
        PollBuild();
        PollPendingRestore();

        // Before the game ticks, so it never sees the Escape press that
        // takes its input away (a game may well bind Escape itself).
        if (GameHasInput() && (State.Play != PlayState::Playing || !_Window->IsFocused() ||
                               _Window->GetInputManager().WasKeyPressed(Input::KeyCode::Escape))) {
            SetGameHasInput(false);
        }

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

    void Editor::LoadTheme(const std::string& Theme) {
        const auto ThemePath = fs::current_path() / "Config" / "Themes" / fs::path(Theme).replace_extension(".json");
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
        Style.TabRounding      = 0.0f;
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
        Colors[ImGuiCol_HeaderHovered]         = Color("#40576f").Lightened(0.1f).To<ImVec4>();
        Colors[ImGuiCol_Header]                = Color("#40576f").To<ImVec4>();
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
                                PropertyEditorReflector Reflector(&_AssetIndex, C);
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

        ImGuiWindowClass Wc;
        Wc.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
        ImGui::SetNextWindowClass(&Wc);

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
            if (SceneTexture != 0) {
                ImGui::Image(SceneTexture, Avail);
                if (State.Play == PlayState::Playing && ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    SetGameHasInput(true);
                }
            }
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

    void Editor::Action_OpenCppProject() {
        if (_EditorSettings.ExternalTools.CodeEditor == CodeEditor::None) return;

        const auto CodeEditorPath = GetCodeEditorPath(_EditorSettings.ExternalTools.CodeEditor);
        if (!CodeEditorPath.has_value()) return;

        if (!exists(*CodeEditorPath) || !exists(_CurrentProject.ProjectRoot / "CMakeLists.txt")) { return; }

        char CmdBuffer[512] {};
        std::snprintf(CmdBuffer,
                      sizeof(CmdBuffer),
                      "%s %s",
                      CodeEditorPath->string().c_str(),
                      _CurrentProject.ProjectRoot.string().c_str());

        STARTUPINFOA StartupInfo;
        PROCESS_INFORMATION ProcessInfo;
        ::ZeroMemory(&StartupInfo, sizeof(StartupInfo));
        ::ZeroMemory(&ProcessInfo, sizeof(ProcessInfo));

        StartupInfo.cb = sizeof(StartupInfo);

        if (::CreateProcessA(CodeEditorPath->string().c_str(),
                             CmdBuffer,
                             nullptr,
                             nullptr,
                             false,
                             0,
                             nullptr,
                             nullptr,
                             &StartupInfo,
                             &ProcessInfo)) {
            LOG_INFO("Opened C++ project in %s", CodeEditorPath->filename().stem().string().c_str());

            ::CloseHandle(ProcessInfo.hProcess);
            ::CloseHandle(ProcessInfo.hThread);
        } else {
            LOG_ERR("Failed to open C++ project (couldn't open code editor)");
        }
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
                                if (ImGui::MenuItem("Save as Prefab...")) {
                                    State.PrefabSourceActor = Entry.Handle;
                                    State.PrefabError.clear();
                                    State.PrefabName.fill('\0');
                                    std::snprintf(State.PrefabName.data(),
                                                  State.PrefabName.size(),
                                                  "%s",
                                                  Entry.Name.c_str());
                                    State.ShowSavePrefabModal = true;
                                }
                                ImGui::Separator();
                                if (ImGui::MenuItem("Duplicate")) { Action_DuplicateActor(S); }
                                if (ImGui::MenuItem("Delete")) { Action_DeleteActor(S); }
                                ImGui::EndPopup();
                            }
                        }
                    }
                    ImGui::EndListBox();

                    // The whole panel takes a prefab dragged in from the
                    // Content Browser.
                    const ImGuiWindow* Panel = ImGui::GetCurrentWindow();
                    if (ImGui::BeginDragDropTargetCustom(Panel->InnerRect, ImGui::GetID("##PrefabDrop"))) {
                        const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(
                          UI::kAssetPayloadType,
                          ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
                        if (Payload && Payload->DataSize == sizeof(UI::AssetDragPayload)) {
                            const auto& Dropped = *CAST<const UI::AssetDragPayload*>(Payload->Data);
                            if (Dropped.Kind == AssetKind::Prefab) {
                                ImGui::GetForegroundDrawList()->AddRect(Panel->InnerRect.Min,
                                                                        Panel->InnerRect.Max,
                                                                        IM_COL32(131, 203, 16, 255),
                                                                        0.0f,
                                                                        0,
                                                                        2.0f);
                                if (Payload->IsDelivery()) {
                                    if (const AssetEntry* Entry = _AssetIndex.Find(AssetID(Dropped.ID))) {
                                        Action_InstantiatePrefab(_AssetIndex.Root() / Entry->RelativePath);
                                    }
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
            }
        }
        ImGui::End();
    }

    void Editor::View_ContentBrowser() {
        _ContentBrowser.Draw(
          _Icons,
          _AssetIndex,
          [this](const fs::path& SceneFile) { LoadSceneFile(SceneFile); },
          [this](const fs::path& PrefabFile) { Action_InstantiatePrefab(PrefabFile); });
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

    void Editor::Action_NewComponentClass() {
        State.ShowNewComponentClassModal = true;
    }

    void Editor::Action_NewScene() const {
        State.ShowNewSceneModal = true;
    }

    void Editor::Action_Quit() {
        const auto Result =
          ::MessageBoxA(_Window->GetHandle(), "Are you sure you want to quit XED?", "XED", MB_YESNO | MB_ICONQUESTION);
        if (Result == IDYES) { _Running = false; }
    }

    void Editor::Action_Play() {
        if (!_EmbeddedGame || !_EmbeddedGame->IsRunning()) return;

        if (State.Play == PlayState::Paused) {
            _EmbeddedGame->SetSimulationEnabled(true);
            State.Play = PlayState::Playing;
            SetGameHasInput(true);
            return;
        }
        if (State.Play == PlayState::Playing) return;

        Scene* Active = _EmbeddedGame->GetActiveScene();
        if (!Active) return;

        try {
            State.PlaySnapshot = SceneSerializer::SaveToJson(*Active);
        } catch (const std::exception& Ex) {
            LOG_ERR("Can't enter play mode - failed to snapshot the scene: %s", Ex.what());
            return;
        }

        const Actor* Selected     = Active->Get(State.SelectedActor);
        State.PlaySelectedActorID = Selected ? Selected->GetActorID() : 0;
        State.PlayScene           = Active;

        _EmbeddedGame->SetSimulationEnabled(true);
        State.Play = PlayState::Playing;
        SetGameHasInput(true);
    }

    void Editor::Action_Pause() {
        if (State.Play != PlayState::Playing || !_EmbeddedGame) return;
        SetGameHasInput(false);
        _EmbeddedGame->SetSimulationEnabled(false);
        State.Play = PlayState::Paused;
    }

    void Editor::Action_Stop() {
        if (State.Play == PlayState::Edit || !_EmbeddedGame) return;

        SetGameHasInput(false);
        _EmbeddedGame->SetSimulationEnabled(false);
        State.Play          = PlayState::Edit;
        State.SelectedActor = ActorHandle::Invalid();

        Scene* Active = _EmbeddedGame->GetActiveScene();
        if (Active && Active == State.PlayScene) {
            try {
                SceneSerializer::LoadFromJson(*Active, State.PlaySnapshot);

                // Restored actors get new handles; find the selection by its persistent ID.
                if (State.PlaySelectedActorID != 0) {
                    Active->ForEachActor([](const Actor& A) {
                        if (A.GetActorID() == State.PlaySelectedActorID) { State.SelectedActor = A.GetHandle(); }
                    });
                }
            } catch (const std::exception& Ex) { LOG_ERR("Failed to restore the scene after play: %s", Ex.what()); }
        } else if (exists(State.CurrentSceneFile)) {
            // Game code switched scenes during play - go back to the one being edited.
            LoadSceneFile(State.CurrentSceneFile);
        }

        State.PlaySnapshot = Json();
        State.PlayScene    = nullptr;
    }

    void Editor::SetGameHasInput(const bool HasInput) {
        const bool GameTakesInput = HasInput && _EmbeddedGame != nullptr;
        _UI.SetInputEnabled(!GameTakesInput);
        if (_EmbeddedGame) _EmbeddedGame->SetInputHost(GameTakesInput ? _Window.get() : nullptr);
    }

    bool Editor::GameHasInput() const {
        return _EmbeddedGame && _EmbeddedGame->HasInputHost();
    }

    void Editor::Action_SaveScene() const {
        // Play-mode changes are temporary and must never reach the scene file.
        if (State.Play != PlayState::Edit) { return; }
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

    void Editor::Action_Compile() {
        StartBuild(BuildTarget::Module);
    }

    void Editor::Action_BuildGame(const BuildTarget Target) {
        StartBuild(Target);
    }

    void Editor::StartBuild(const BuildTarget Target) {
        if (_CurrentProject.Name.empty() || _Build.IsRunning()) return;

        // The project's own XenBuild.bat (so it can be customised). Projects
        // made before it existed get the current one from XED's templates.
        const fs::path Script = _CurrentProject.ProjectRoot / "XenBuild.bat";
        std::error_code Ec;
        if (!exists(Script, Ec)) {
            fs::copy_file("Templates/XenBuild.bat", Script, Ec);
            if (Ec) {
                LOG_ERR("Build: the project has no XenBuild.bat and copying one from the templates failed: %s",
                        Ec.message().c_str());
                return;
            }
        }

        const char* Arg = "module";
        switch (Target) {
            case BuildTarget::Module:
                Arg = "module";
                break;
            case BuildTarget::Debug:
                Arg = "debug";
                break;
            case BuildTarget::Release:
                Arg = "release";
                break;
        }

        // cmd /c "<script>" <target>: the whole thing wrapped in one more pair of quotes, as cmd requires.
        const std::wstring CommandLine =
          L"cmd.exe /c \"\"" + Script.wstring() + L"\" " + std::wstring(Arg, Arg + std::strlen(Arg)) + L"\"";
        std::string Error;
        if (!_Build.Start(CommandLine, _CurrentProject.ProjectRoot, Error)) {
            LOG_ERR("Build: %s", Error.c_str());
            return;
        }
        _BuildProjectRoot = _CurrentProject.ProjectRoot;
        _BuildTarget      = Target;
        LOG_INFO("Building '%s' (XenBuild.bat %s)...", _CurrentProject.Name.c_str(), Arg);
    }

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

    void Editor::Action_InstantiatePrefab(const fs::path& PrefabFile) const {
        Scene* Active = _EmbeddedGame ? _EmbeddedGame->GetActiveScene() : nullptr;
        if (!Active) {
            LOG_WARN("Can't instantiate '%s': no scene is open", PrefabFile.filename().string().c_str());
            return;
        }

        try {
            std::ifstream In(PrefabFile);
            if (!In) { THROW_ENGINE_EXCEPTION(SerializationException, "could not open " + PrefabFile.string()); }
            std::ostringstream Buffer;
            Buffer << In.rdbuf();

            const ActorHandle Root = SceneSerializer::InstantiatePrefab(*Active, Json::parse(Buffer.str()));
            if (Root.IsSet()) State.SelectedActor = Root;
        } catch (const std::exception& Error) {
            LOG_ERR("Couldn't instantiate prefab '%s': %s", PrefabFile.filename().string().c_str(), Error.what());
        }
    }

    void Editor::Modal_SaveAsPrefab() {
        UI::CenterNextWindow();
        if (!ImGui::BeginPopupModal("Save as Prefab", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

        Scene* Active       = _EmbeddedGame ? _EmbeddedGame->GetActiveScene() : nullptr;
        const Actor* Source = Active ? Active->Get(State.PrefabSourceActor) : nullptr;
        if (!Source) {
            // Gone since the menu was opened (deleted, scene switched).
            ImGui::TextDisabled("The actor no longer exists.");
            if (ImGui::Button("Close", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        const fs::path PrefabsDir = _CurrentProject.ContentDirectory / "prefabs";
        const std::string Name    = State.PrefabName.data();

        ImGui::Text("Save '%s' and its children as a prefab", Source->GetName().c_str());
        ImGui::Spacing();

        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(260.0f);
        const bool Submitted =
          ImGui::InputText("##prefabname",
                           State.PrefabName.data(),
                           State.PrefabName.size(),
                           ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        ImGui::SameLine();
        ImGui::TextDisabled(".xprefab");

        // The file name rules the content browser's rename uses.
        std::string Problem;
        if (Name.empty()) Problem = "Give the prefab a name.";
        else if (Name.find_first_of("<>:\"/\\|?*") != std::string::npos)
            Problem = "That name has characters a file can't.";
        else if (Name.back() == ' ' || Name.back() == '.') Problem = "A name can't end with a space or a dot.";

        const fs::path Target = PrefabsDir / (Name + ".xprefab");
        const bool Exists     = Problem.empty() && fs::exists(Target);

        ImGui::TextDisabled("Saved to Content/prefabs/");
        if (!Problem.empty()) ImGui::TextColored(ImVec4(0.92f, 0.22f, 0.32f, 1.0f), "%s", Problem.c_str());
        if (Exists)
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "A prefab with this name exists and will be replaced.");
        if (!State.PrefabError.empty())
            ImGui::TextColored(ImVec4(0.92f, 0.22f, 0.32f, 1.0f), "%s", State.PrefabError.c_str());
        ImGui::Spacing();

        ImGui::BeginDisabled(!Problem.empty());
        const bool Confirm =
          ImGui::Button(Exists ? "Replace" : "Save", ImVec2(120, 0)) || (Submitted && Problem.empty());
        ImGui::EndDisabled();
        ImGui::SameLine();
        const bool Cancel = ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false);

        if (Confirm) {
            try {
                SceneSerializer::SavePrefabToFile(*Active, State.PrefabSourceActor, Target);
                LOG_INFO("Saved prefab '%s'", Target.string().c_str());

                // Both the asset index (inspector slots, drop targets) and the browser's folder listing
                _AssetIndex.Rescan(_CurrentProject.ContentDirectory);
                _ContentBrowser.NotifyContentChanged();
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& Error) { State.PrefabError = std::string("Couldn't save: ") + Error.what(); }
        } else if (Cancel) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    void Editor::Modal_AddComponent() const {
        UI::CenterNextWindow();

        if (ImGui::BeginPopupModal("Add Component", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Select which component to add");
            ImGui::Separator();

            if (ImGui::BeginListBox("##Components", ImVec2(260.f, 220.f))) {
                for (const auto& Name : GetAddableComponentNames()) {
                    if (ImGui::Selectable(Name.c_str(), State.AddComponentChoice == Name)) {
                        State.AddComponentChoice = Name;
                    }
                }
                ImGui::EndListBox();
            }

            ImGui::BeginDisabled(State.AddComponentChoice.empty());
            if (ImGui::Button("Add", ImVec2(120, 0))) {
                if (_EmbeddedGame && _EmbeddedGame->GetActiveScene()) {
                    AddComponentByName(_EmbeddedGame->GetActiveScene()->Get(State.SelectedActor),
                                       State.AddComponentChoice);
                }
                State.AddComponentChoice.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                State.AddComponentChoice.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void Editor::Modal_NewComponentClass() {
        UI::CenterNextWindow();

        static std::array<char, MAX_PATH> ClassName {};

        if (ImGui::BeginPopupModal("New Component Class", nullptr)) {
            const std::string ClassNameStr = ClassName.data();
            ImGui::InputText("Name", ClassName.data(), ClassName.size());

            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ClassName.fill(0);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            ImGui::BeginDisabled(ClassNameStr.empty());
            if (ImGui::Button("Create", ImVec2(120, 0))) {
                CreateComponentClass(ClassNameStr);
                ClassName.fill(0);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();

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
        RegisterShortcut(ImGuiMod_Ctrl | ImGuiKey_B, [this] { Action_Compile(); });
        RegisterShortcut(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_O, [this] { Action_OpenScene(); });
        RegisterShortcut(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_N, [this] { Action_NewScene(); });
    }

    void Editor::DrawMainMenuBar() {
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

            const bool SaveOptionsEnabled = State.Play == PlayState::Edit && _EmbeddedGame != nullptr &&
                                            _EmbeddedGame->GetActiveScene() != nullptr &&
                                            exists(State.CurrentSceneFile);
            if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, SaveOptionsEnabled)) { Action_SaveScene(); }
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S", false, SaveOptionsEnabled)) {
                Action_SaveSceneAs();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Open C++ Project", nullptr, false, !_CurrentProject.Name.empty())) {
                Action_OpenCppProject();
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

            if (ImGui::MenuItem("Project Settings", nullptr, false, true)) {}
            if (ImGui::MenuItem("Editor Settings", "Ctrl+Alt+S", false, true)) { Action_ShowSettings(); }

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
                for (const auto& Item : GetAddableComponentNames()) {
                    if (ImGui::MenuItem(Item.c_str(), nullptr, false, true)) {
                        AddComponentByName(_EmbeddedGame->GetActiveScene()->Get(State.SelectedActor), Item);
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Component Class...", nullptr, false, true)) { Action_NewComponentClass(); }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Build", !_CurrentProject.Name.empty())) {
            if (ImGui::MenuItem("Compile Game Module", "Ctrl+B", false, !_Build.IsRunning())) { Action_Compile(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Build Game (Debug)", nullptr, false, !_Build.IsRunning())) {
                Action_BuildGame(BuildTarget::Debug);
            }
            if (ImGui::MenuItem("Build Game (Release)", nullptr, false, !_Build.IsRunning())) {
                Action_BuildGame(BuildTarget::Release);
            }
            ImGui::Separator();
            ImGui::MenuItem("Rebuild", nullptr, false, false);
            ImGui::MenuItem("Clean", nullptr, false, false);
            ImGui::EndMenu();
        }

        static bool ShowGenAssetIDModal = false;
        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Generate Asset ID", nullptr, false, true)) { ShowGenAssetIDModal = true; }
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

        if (State.ShowNewComponentClassModal) {
            ImGui::OpenPopup("New Component Class");
            State.ShowNewComponentClassModal = false;
        }

        if (State.ShowSavePrefabModal) {
            ImGui::OpenPopup("Save as Prefab");
            State.ShowSavePrefabModal = false;
        }

        if (ShowGenAssetIDModal) {
            ImGui::OpenPopup("Generate Asset ID");
            ShowGenAssetIDModal = false;
        }

        _SettingsModal.Draw(this);
        Modal_NewProject();
        Modal_NewScene();
        Modal_NewComponentClass();
        Modal_SaveAsPrefab();

        static char AssetPath[MAX_STR_LEN] {};
        static char GeneratedID[MAX_STR_LEN] {};
        UI::CenterNextWindow();
        if (ImGui::BeginPopupModal("Generate Asset ID", nullptr)) {
            ImGui::InputText("Asset Path", AssetPath, MAX_STR_LEN);
            ImGui::InputText("Generated ID", GeneratedID, MAX_STR_LEN);

            if (std::strlen(AssetPath) > 0) {
                const AssetID ID = PAK::HashPath(PAK::Canonicalize(AssetPath));
                const auto IDStr = std::to_string(ID.Value);
                std::strcpy(GeneratedID, IDStr.c_str());
            }

            if (ImGui::Button("Close", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }

            ImGui::EndPopup();
        }
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

            if (UI::Controls::ToolbarButton("##OpenFolder", _Icons.Get(EditorIcon::OpenFolder), IconSize, true)) {
                Action_OpenProject();
            }
            ImGui::SetItemTooltip("Open Project");

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 2.0f);
            UI::Controls::ToolbarButton("##Undo", _Icons.Get(EditorIcon::Undo), IconSize, false);
            UI::Controls::ToolbarButton("##Redo", _Icons.Get(EditorIcon::Redo), IconSize, false);

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 2.0f);
            UI::Controls::ToolbarButton("##Select", _Icons.Get(EditorIcon::Select), IconSize, false);
            UI::Controls::ToolbarButton("##Move", _Icons.Get(EditorIcon::Move), IconSize, false);
            UI::Controls::ToolbarButton("##Rotate", _Icons.Get(EditorIcon::Rotate), IconSize, false);
            UI::Controls::ToolbarButton("##Scale", _Icons.Get(EditorIcon::Scale), IconSize, false);
            UI::Controls::ToolbarButton("##FocusSelected", _Icons.Get(EditorIcon::FocusSelected), IconSize, false);
            UI::Controls::ToolbarButton("##SelectAsset", _Icons.Get(EditorIcon::SelectAsset), IconSize, false);
            UI::Controls::ToolbarButton("##GridToggle", _Icons.Get(EditorIcon::GridToggle), IconSize, false);

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 2.0f);
            const bool SceneActive = _EmbeddedGame && _EmbeddedGame->IsRunning() && _EmbeddedGame->GetActiveScene();
            if (UI::Controls::ToolbarButton("##Play",
                                            _Icons.Get(EditorIcon::Play),
                                            IconSize,
                                            SceneActive && State.Play != PlayState::Playing)) {
                Action_Play();
            }
            ImGui::SetItemTooltip(State.Play == PlayState::Paused ? "Resume" : "Play");
            UI::Controls::ToolbarButton("##PlayWindowed", _Icons.Get(EditorIcon::PlayWindowed), IconSize, false);
            if (UI::Controls::ToolbarButton("##Pause",
                                            _Icons.Get(EditorIcon::Pause),
                                            IconSize,
                                            State.Play == PlayState::Playing)) {
                Action_Pause();
            }
            ImGui::SetItemTooltip("Pause");
            if (UI::Controls::ToolbarButton("##Stop",
                                            _Icons.Get(EditorIcon::Stop),
                                            IconSize,
                                            State.Play != PlayState::Edit)) {
                Action_Stop();
            }
            ImGui::SetItemTooltip("Stop (restores the scene)");

            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 2.0f);
            ImGui::BeginDisabled(_Build.IsRunning() || _CurrentProject.Name.empty());
            if (UI::Controls::ToolbarButton("##CompileCode", _Icons.Get(EditorIcon::CompileCode), IconSize)) {
                Action_Compile();
            }
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("Build the game module and reload it (Ctrl+B)");

            UI::Controls::ToolbarButton("##CleanCode", _Icons.Get(EditorIcon::CleanCode), IconSize, false);
            ImGui::SetItemTooltip("Clean Code");
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
