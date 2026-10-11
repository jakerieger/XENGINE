//
// Created by Jake Rieger on 9/27/2026.
//

#pragma once

#include <Xen/Window.hpp>
#include <Xen/RenderDevice.hpp>
#include <Xen/CommandBuffer.hpp>
#include <Xen/Game.hpp>
#include <Xen/GameModule.hpp>

#include "AssetIndex.hpp"
#include "BuildProcess.hpp"
#include "ContentBrowser.hpp"
#include "EditorSettings.hpp"
#include "EditorSettingsModal.hpp"
#include "EditorTheme.hpp"
#include "EditorUI.hpp"
#include "EditorProject.hpp"
#include "IconLibrary.hpp"
#include "ProjectSettingsModal.hpp"

#include <Common/Platform.hpp>
#include <functional>
#include <memory>
#include <vector>

namespace Xen {
    DEFINE_ENGINE_EXCEPTION(EditorException);

    class Editor {
    public:
        void LoadEditorFonts() const;
        Editor();
        ~Editor();

        Editor(const Editor&)            = delete;
        Editor& operator=(const Editor&) = delete;

        void Run();

    private:
        friend EditorSettingsModal;
        friend ProjectSettingsModal;
        friend ContentBrowser;

        static constexpr size_t MAX_STR_LEN = 512;

        enum class CreateProjectResult : u8 {
            Success = 0,
            AlreadyExists,
            Failed,
        };

        void LoadProject(const fs::path& PrxjPath);

        /// Finds the project's built game module (<project>/build/**/<Name>Module.dll,
        /// newest wins), copies it to a fresh temp folder - Windows locks a
        /// loaded DLL, and loading the original would block rebuilding it -
        /// and loads the copy. Returns false (with the reason logged) when
        /// there's no module or it's unusable; the project then runs on the
        /// base Game.
        bool LoadGameModule();

        /// Destroys the embedded Game and unloads the module, in that order.
        void UnloadProject();

        /// Loads the project's module (if built) and creates + starts the
        /// embedded Game from it, in edit mode.
        void StartEmbeddedGame();

        /// Hot reload: Action_Compile runs the project's 'editor' preset build
        /// as a child process, streaming its output into the Log; PollBuild
        /// (every frame, before the frame bracket) reports it, and on success
        /// ReloadGameModule swaps in the new module - snapshot the scene,
        /// destroy the Game, unload, load the new copy, restore the scene.
        void PollBuild();
        void ReloadGameModule();
        void PollPendingRestore();
        static CreateProjectResult CreateProject(const std::string& Name, const fs::path& Dir);
        void CreateComponentClass(const std::string& ClassName);

        void LoadSceneFile(const fs::path& SceneFile) const;
        void CreateScene(const std::string& Name, const fs::path& SceneFile) const;

        void SetWindowTitle(const std::string& Title) const;
        void TickFrame(f32 DeltaTime);
        void DrawDockspaceAndPanels(f32 DeltaTime);
        void DrawMainMenuBar();
        void DrawToolbar();

        // Width/Height rather than an ImVec2, so this header (like EditorUI.hpp/
        // DebugUI.hpp) doesn't need to pull in Dear ImGui's own headers just to
        // declare it.
        static void EnsureDefaultLayout(unsigned int DockspaceID, f32 Width, f32 Height);

        void LoadTheme(const std::string& Theme);
        void ApplyCurrentTheme() const;

        // Each view or panel in the editor UI
        void View_ContentBrowser();
        void View_Hierarchy() const;
        void View_Inspector() const;
        void View_Log() const;
        void View_Scene(f32 DeltaTime);

        // Dedicated methods for main menu actions so they can be called independently (i.e. for keyboard shortcuts)
        /// What a build produces: the module XED loads ('editor' preset), or a
        /// Debug/Release build of the game itself (exe + packed content).
        enum class BuildTarget : u8 { Module, Debug, Release };

        void Action_Compile();  // BuildTarget::Module
        void Action_BuildGame(BuildTarget Target);
        void StartBuild(BuildTarget Target);
        void Action_DeleteActor(Scene* S) const;
        void Action_DuplicateActor(Scene* S) const;

        /// Spawns a copy of a prefab file into the open scene, at the
        /// transform it was saved with, and selects it.
        void Action_InstantiatePrefab(const fs::path& PrefabFile) const;
        void Action_NewActor(Scene* S, const std::string& Name) const;
        void Action_NewProject() const;
        void Action_OpenCppProject();
        void Action_OpenProject();
        void Action_OpenScene() const;
        void Action_ShowSettings();
        void Action_NewComponentClass();
        void Action_NewScene() const;
        void Action_Quit();
        void Action_SaveScene() const;
        void Action_Play();
        void Action_Pause();
        void Action_Stop();
        void Action_SaveSceneAs();

        // Play-mode input ownership: while the game has input, the editor's
        // UI is deaf and the embedded game reads the window's keyboard and
        // mouse. Play hands it to the game; Escape (or the window losing
        // focus) hands it back; clicking the Scene view while playing hands
        // it to the game again.
        void SetGameHasInput(bool HasInput);
        NODISCARD bool GameHasInput() const;

        void Modal_AddComponent() const;
        void Modal_NewComponentClass();
        void Modal_NewProject();
        void Modal_NewScene() const;
        void Modal_SaveAsPrefab();

        // Keyboard shortcuts: register once (typically in the constructor)
        // with RegisterShortcut(Keys, Action), then ProcessShortcuts() fires
        // whichever ones were pressed this frame - called once per frame
        // from DrawDockspaceAndPanels, outside any specific panel's Begin/
        // End so the shortcuts are global (see ProcessShortcuts' own comment
        // on ImGuiInputFlags_RouteGlobal) rather than only firing while one
        // particular window happens to have focus.
        //
        // Keys is an ImGuiKeyChord (see imgui.h) - e.g. ImGuiMod_Ctrl |
        // ImGuiKey_O - kept as a plain int here so this header, like
        // EditorUI.hpp/DebugUI.hpp, doesn't need Dear ImGui's own headers
        // just to declare this.
        void RegisterShortcut(int Keys, std::function<void()> Action);
        void ProcessShortcuts() const;
        void SetupShortcuts();

        struct EditorShortcut {
            int Keys;
            std::function<void()> Action;
        };
        std::vector<EditorShortcut> _Shortcuts;

        EditorSettings _EditorSettings {};
        EditorProject _CurrentProject {};
        AssetIndex _AssetIndex;
        ContentBrowser _ContentBrowser;
        EditorTheme _CurrentTheme {};
        std::unique_ptr<Window> _Window;

        EditorSettingsModal _SettingsModal;

        // Destroyed in reverse declaration order: _Icons first (its
        // Shutdown calls _Device->DestroyTexture, so it must go while
        // _Device is still alive - see IconLibrary.hpp), then _UI (its own
        // WaitIdle backstop still has a live _Device either way - see
        // Game.hpp's identical ordering comment), then _EmbeddedGame -
        // which only ever borrows _Device and must be torn down while it's
        // still alive - and _Device last.
        std::unique_ptr<RHI::IRenderDevice> _Device;

        // The project's game module, when it has one. Declared before
        // _EmbeddedGame so it's destroyed after it: the Game's vtable and
        // destructor are code inside the module's DLL.
        GameModule _GameModule;

        // A Game created by the module must be destroyed by it too.
        struct GameDeleter {
            const GameModule* Module {nullptr};
            void operator()(Game* G) const {
                if (Module) {
                    Module->DestroyGame(G);
                } else {
                    delete G;
                }
            }
        };
        std::unique_ptr<Game, GameDeleter> _EmbeddedGame;
        EditorUI _UI;
        IconLibrary _Icons;

        // The editor's own "base layer" clear, submitted before _UI's
        // overlay draws the real UI on top of it - see Run()'s own comment.
        RHI::CommandBuffer _Commands;

        BuildProcess _Build;
        fs::path _BuildProjectRoot;
        BuildTarget _BuildTarget {BuildTarget::Module};

        u32 _SceneViewportWidth {0};
        u32 _SceneViewportHeight {0};

        bool _Running {false};
    };
}  // namespace Xen
