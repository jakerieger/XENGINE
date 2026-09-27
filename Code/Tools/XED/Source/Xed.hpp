//
// Created by Jake Rieger on 9/27/2026.
//

#pragma once

#include <Xen/Window.hpp>
#include <Xen/DebugUI.hpp>
#include <Xen/RenderDevice.hpp>
#include <Xen/CommandBuffer.hpp>
#include <Xen/Game.hpp>

#include <filesystem>
#include <memory>

namespace Xen {
    DEFINE_ENGINE_EXCEPTION(EditorException);

    /// @brief Editor slice 1: a docked ImGui UI around a "Scene" panel that
    /// displays an embedded Xen::Game's own Viewport - not a project/content
    /// system yet (a hardcoded test content dir, see CMakeLists.txt), not
    /// play-in-editor input routing. One IRenderDevice and one DebugUI for
    /// the whole editor, shared with (but not owned by) the embedded Game -
    /// see Game.hpp's editor constructor for why.
    class XED {
    public:
        XED();
        ~XED();

        XED(const XED&)            = delete;
        XED& operator=(const XED&) = delete;

        void Run();

    private:
        void TickFrame(f32 DeltaTime);
        void DrawDockspaceAndPanels();
        void EnsureDefaultLayout(unsigned int DockspaceID) const;

        /// @brief Hand-builds a small scene (mesh+material, ground, camera,
        /// directional light, environment) using only core Xen components -
        /// deliberately NOT Sandbox's own main.xscene, which references
        /// Sandbox-specific component types (RotatingComponent) that only
        /// Sandbox's own executable links in and registers. A data-only
        /// project (see the editor plan) can't assume an arbitrary scene's
        /// custom component types exist in the editor process at all - this
        /// is a placeholder until the real project/content system (and,
        /// eventually, a loadable game module) exists. Saves to ScratchPath
        /// and returns it, for LoadSceneFromFile.
        std::filesystem::path BuildTestScene(const EngineContext& Ctx) const;

        std::unique_ptr<Window> _EditorWindow;

        // Destroyed in reverse declaration order: _DebugUI first (its own
        // WaitIdle backstop still has a live _Device either way - see
        // Game.hpp's identical ordering comment), then _EmbeddedGame - which
        // only ever borrows _Device and must be torn down while it's still
        // alive - and _Device last.
        std::unique_ptr<RHI::IRenderDevice> _Device;
        std::unique_ptr<Game> _EmbeddedGame;
        DebugUI _DebugUI;

        // The editor's own "base layer" clear, submitted before DebugUI's
        // overlay draws the real UI on top of it - see Run()'s own comment.
        RHI::CommandBuffer _Commands;

        u32 _SceneViewportWidth {0};
        u32 _SceneViewportHeight {0};

        bool _Running {false};
    };
}  // namespace Xen
