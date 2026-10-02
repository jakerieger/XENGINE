//
// Created by Jake Rieger on 9/27/2026.
//

#pragma once

#include <Xen/Window.hpp>
#include <Xen/RenderDevice.hpp>
#include <Xen/CommandBuffer.hpp>
#include <Xen/Game.hpp>

#include "EditorSettings.hpp"
#include "EditorSettingsModal.hpp"
#include "EditorTheme.hpp"
#include "EditorUI.hpp"
#include "EditorProject.hpp"
#include "IconLibrary.hpp"

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
        enum class CreateProjectResult : u8 {
            Success = 0,
            AlreadyExists,
            Failed,
        };

        void LoadProject(const fs::path& PrxjPath);
        CreateProjectResult CreateProject(const std::string& Name, const fs::path& Dir) const;

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
        void EnsureDefaultLayout(unsigned int DockspaceID, f32 Width, f32 Height) const;

        void LoadTheme(const std::string& ThemeFile);
        void ApplyCurrentTheme() const;

        // Each view or panel in the editor UI
        void View_ContentBrowser() const;
        void View_Hierarchy() const;
        void View_Inspector() const;
        void View_Log() const;
        void View_Scene(f32 DeltaTime);

        // Dedicated methods for main menu actions so they can be called independently (i.e. for keyboard shortcuts)
        void Action_DeleteActor(Scene* S) const;
        void Action_DuplicateActor(Scene* S) const;
        void Action_NewActor(Scene* S, const std::string& Name) const;
        void Action_NewProject() const;
        void Action_OpenProject();
        void Action_OpenScene() const;
        void Action_ShowSettings();
        void Action_NewScene() const;
        void Action_Quit();
        void Action_Save() const;
        void Action_SaveAs();

        void Modal_AddComponent() const;
        void Modal_NewProject();
        void Modal_NewScene() const;

        void CenterNextWindow() const;

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
        std::unique_ptr<Game> _EmbeddedGame;
        EditorUI _UI;
        IconLibrary _Icons;

        // The editor's own "base layer" clear, submitted before _UI's
        // overlay draws the real UI on top of it - see Run()'s own comment.
        RHI::CommandBuffer _Commands;

        u32 _SceneViewportWidth {0};
        u32 _SceneViewportHeight {0};

        bool _Running {false};
    };
}  // namespace Xen
