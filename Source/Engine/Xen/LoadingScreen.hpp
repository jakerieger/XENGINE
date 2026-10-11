//
// Created by Jake Rieger on 9/20/2026.
//
// The engine's built-in loading screen: a background and a spinner, drawn
// straight into the swap chain by its own tiny pipeline - so it needs
// no asset from a pak (nothing is loaded yet when it has to appear), no font
// (there's no text rendering outside the debug-only ImGui) and no Scene. A game
// that wants its own look overrides Game::OnLoadingScreen.

#pragma once

#include <Common/XenCommon.hpp>

#include "RenderDevice.hpp"

namespace Xen {
    /// @brief How far a load has got, handed to the loading screen (and to
    /// Game::OnLoadingScreen) each frame.
    struct LoadingProgress {
        /// 0..1: finished assets over total. The built-in screen doesn't show
        /// it; it's here for a game's own OnLoadingScreen.
        f32 Fraction {0.0f};
        size_t Done {0};
        size_t Total {0};
        f32 ElapsedSeconds {0.0f};
    };

    class LoadingScreen {
    public:
        struct Color {
            f32 R {0.0f}, G {0.0f}, B {0.0f}, A {1.0f};
        };

        struct Config {
            Color Background {0.045f, 0.05f, 0.065f, 1.0f};
            Color Accent {1.f, 1.f, 1.0f, 1.0f};

            /// A load has to run this long before the screen appears at all,
            /// so a quick scene change stays instant instead of flashing it.
            f32 ShowDelaySeconds {0.3f};

            /// Once it has appeared it stays at least this long, so a load
            /// that finishes just after the delay doesn't flash it for a few
            /// frames.
            f32 MinVisibleSeconds {0.4f};
        };

        LoadingScreen() = default;
        ~LoadingScreen();

        LoadingScreen(const LoadingScreen&)            = delete;
        LoadingScreen& operator=(const LoadingScreen&) = delete;

        bool Initialize(RHI::IRenderDevice& Device);
        void Shutdown();

        NODISCARD bool IsInitialized() const { return _Device != nullptr; }
        NODISCARD Config& GetConfig() { return _Config; }
        NODISCARD const Config& GetConfig() const { return _Config; }

        /// @brief Restarts the spinner's angle - call when a new load begins
        /// to show.
        void Reset();

        /// @brief Just the cleared background. Records and submits into the
        /// device's current frame (call between BeginFrame and EndFrame) -
        /// used at startup so the window is never a blank rectangle.
        void DrawBackground();

        /// @brief Background and spinner, into the current frame (between
        /// BeginFrame and EndFrame). DeltaTime advances the spinner.
        void Draw(const LoadingProgress& Progress, f32 DeltaTime);

    private:
        void DrawShapes(const LoadingProgress& Progress, f32 DeltaTime, bool WithShapes);

        RHI::IRenderDevice* _Device {nullptr};
        RHI::LayoutHandle _Layout {};
        RHI::PipelineHandle _Pipeline {};
        RHI::CommandBuffer _Commands;

        Config _Config {};
        f32 _SpinnerAngle {0.0f};
    };
}  // namespace Xen
