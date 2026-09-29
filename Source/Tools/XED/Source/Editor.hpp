//
// Created by Jake Rieger on 9/27/2026.
//

#pragma once

#include <Xen/Window.hpp>
#include <Xen/RenderDevice.hpp>
#include <Xen/CommandBuffer.hpp>
#include <Xen/Game.hpp>

#include "EditorConfig.hpp"
#include "EditorTheme.hpp"
#include "EditorUI.hpp"
#include "EditorProject.hpp"

#include <filesystem>
#include <memory>

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
        void LoadProject(const std::filesystem::path& PrxjPath);

    private:
        void TickFrame(f32 DeltaTime);
        void DrawDockspaceAndPanels(f32 DeltaTime);
        void EnsureDefaultLayout(unsigned int DockspaceID) const;

        void LoadTheme(const std::string& ThemeFile);
        void ApplyCurrentTheme() const;

        // Each view or panel in the editor UI
        void View_Inspector() const;
        void View_Scene(f32 DeltaTime);
        void View_Hierarchy() const;
        void View_ContentBrowser() const;
        void View_Log() const;

        EditorConfig _Config {};
        EditorProject _CurrentProject {};
        EditorTheme _CurrentTheme {};
        std::unique_ptr<Window> _Window;

        // Destroyed in reverse declaration order: _UI first (its own
        // WaitIdle backstop still has a live _Device either way - see
        // Game.hpp's identical ordering comment), then _EmbeddedGame - which
        // only ever borrows _Device and must be torn down while it's still
        // alive - and _Device last.
        std::unique_ptr<RHI::IRenderDevice> _Device;
        std::unique_ptr<Game> _EmbeddedGame;
        EditorUI _UI;

        // The editor's own "base layer" clear, submitted before _UI's
        // overlay draws the real UI on top of it - see Run()'s own comment.
        RHI::CommandBuffer _Commands;

        u32 _SceneViewportWidth {0};
        u32 _SceneViewportHeight {0};

        bool _Running {false};
    };
}  // namespace Xen
