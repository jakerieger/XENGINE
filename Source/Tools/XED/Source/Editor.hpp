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
#include "Project.hpp"

#include <filesystem>
#include <memory>

namespace Xen {
    DEFINE_ENGINE_EXCEPTION(EditorException);

    /// @brief Editor slice 1: a docked ImGui UI around a "Scene" panel that
    /// displays an embedded Game's own Viewport - not a project/content
    /// system yet (a hardcoded test content dir, see CMakeLists.txt), not
    /// play-in-editor input routing. One IRenderDevice and one EditorUI for
    /// the whole editor, shared with (but not owned by) the embedded Game -
    /// see Game.hpp's editor constructor for why. EditorUI is NOT DebugUI -
    /// the engine's own debug overlay is compiled out of a shippable Release
    /// build, which an editor's own UI cannot tolerate, so the editor owns a
    /// completely separate, always-on UI class instead (see EditorUI.hpp).
    class Editor {
    public:
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

        // Each view or panel in the editor UI
        void View_Inspector() const;
        void View_Scene(f32 DeltaTime);
        void View_Hierarchy() const;
        void View_ContentBrowser() const;
        void View_Log() const;

        EditorConfig _Config {};
        Project _CurrentProject {};
        std::unique_ptr<Window> _EditorWindow;

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
