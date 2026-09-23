//
// Created by Jake Rieger on 9/17/2026.
//

#pragma once

#include "RenderDevice.hpp"

namespace Xen {
    /// @brief Owns an offscreen color target that a scene renders into,
    /// sized independently of the swap chain or window.
    ///
    /// This is the one render-target abstraction the engine renders through -
    /// there is no "render straight to the swap chain" path for gameplay
    /// content. Standalone play is the degenerate case of exactly one
    /// Viewport kept the same size as the window and presented by copying
    /// its texture into the back buffer (IRenderDevice::CopyToSwapChain). An
    /// editor is the general case: any number of Viewports, each a different
    /// size, none of them the size of the window, each displayed by sampling
    /// its color target into an ImGui panel instead of copying it anywhere.
    /// Building on one abstraction from the start means standalone-game
    /// needs no separate render path later when the editor arrives.
    class Viewport {
    public:
        Viewport() = default;
        ~Viewport();

        // Not move-enabled either: _ColorTarget is a handle into the device's
        // pool, not a unique_ptr, so a default move would leave both the
        // moved-from and moved-to Viewport thinking they own it. Held as a
        // plain member (see Game::_MainViewport) - never needs relocating.
        Viewport(const Viewport&)            = delete;
        Viewport& operator=(const Viewport&) = delete;
        Viewport(Viewport&&)                 = delete;
        Viewport& operator=(Viewport&&)      = delete;

        /// @brief Creates the color target (and, if WithDepth, a depth
        /// target sized to match). Width and Height must both be non-zero -
        /// a viewport with no on-screen presence yet (an editor panel not
        /// laid out) should stay uninitialized rather than being created at
        /// some placeholder size and immediately resized.
        ///
        /// ColorFormat defaults to BGRA8_UNORM, matching the swap chain's own
        /// back buffer format - that's what makes CopyToSwapChain a plain GPU
        /// copy for the standalone-game presentation path rather than
        /// needing a format-converting shader. A viewport that will only
        /// ever be sampled (an editor panel, never copied to a swap chain)
        /// has no such constraint and may pass a different format.
        ///
        /// WithDepth is off by default - 2D content (XenPong) never depth-
        /// tests, so it costs a texture for nothing. 3D content needs it on.
        ///
        /// ClearColor is purely a performance hint (TextureDesc::
        /// OptimizedClear) for whoever actually clears this color target
        /// every frame - typically SpriteRenderer, whose own
        /// Config::ClearColor default this matches. It doesn't have to be
        /// exact: a mismatch just falls back to a slower generic clear
        /// (D3D12 debug-layer warning 820), the same as leaving this at its
        /// default entirely - so only worth passing something else here if
        /// a game also calls SpriteRenderer::SetClearColor to something
        /// different.
        bool Initialize(RHI::IRenderDevice& Device,
                        u32 Width,
                        u32 Height,
                        RHI::Format ColorFormat = RHI::Format::BGRA8_UNORM,
                        bool WithDepth          = false,
                        RHI::Format DepthFormat = RHI::Format::D32_FLOAT,
                        f32 ClearR = 0.1f,
                        f32 ClearG = 0.1f,
                        f32 ClearB = 0.1f,
                        f32 ClearA = 1.0f);
        void Shutdown();

        NODISCARD bool IsInitialized() const { return _Device != nullptr; }

        /// @brief Recreates the color target (and depth target, if this
        /// viewport has one) at the new size. A no-op if the size is
        /// unchanged or is zero in either dimension - same guard
        /// IRenderDevice::SetSwapChainSize uses, since a minimized window
        /// reports a 0x0 client area and a 0-sized texture is invalid.
        void Resize(u32 Width, u32 Height);

        NODISCARD RHI::TextureHandle GetColorTarget() const { return _ColorTarget; }
        NODISCARD RHI::Format GetColorFormat() const { return _ColorFormat; }

        NODISCARD bool HasDepth() const { return _DepthTarget.IsValid(); }
        NODISCARD RHI::TextureHandle GetDepthTarget() const { return _DepthTarget; }
        NODISCARD RHI::Format GetDepthFormat() const { return _DepthFormat; }

        NODISCARD u32 GetWidth() const { return _Width; }
        NODISCARD u32 GetHeight() const { return _Height; }
        NODISCARD f32 GetAspectRatio() const {
            return _Height > 0 ? CAST<f32>(_Width) / CAST<f32>(_Height) : 1.0f;
        }

    private:
        RHI::IRenderDevice* _Device {nullptr};
        RHI::TextureHandle _ColorTarget {};
        RHI::Format _ColorFormat {RHI::Format::BGRA8_UNORM};
        RHI::TextureHandle _DepthTarget {};
        RHI::Format _DepthFormat {RHI::Format::D32_FLOAT};
        u32 _Width {0};
        u32 _Height {0};

        // Remembered from Initialize so Resize can recreate _ColorTarget
        // with the same optimized clear value (see Initialize's own
        // comment) rather than silently dropping back to TextureDesc's bare
        // default.
        f32 _ClearColor[4] {0.1f, 0.1f, 0.1f, 1.0f};
    };
}  // namespace Xen
