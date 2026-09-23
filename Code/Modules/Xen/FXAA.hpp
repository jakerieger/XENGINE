//
// Created by Jake Rieger on 9/23/2026.
//
// The last step of every frame, after PostProcess's composite has already
// tonemapped and blended the 3D scene onto the Viewport's color target (see
// Game::TickFrame): a single fullscreen-triangle pass (Code/Shaders/FXAA.hlsl,
// Timothy Lottes' original NVIDIA algorithm) that smooths whatever sharp luma
// edges are left in the final LDR image, geometric or shading - a jagged
// silhouette and an aliased specular highlight both just look like "a sharp
// edge" to it, which is exactly why it helps the highlight-flicker problems
// bloom's Karis average and the shadow map's dithered PCF could each only
// partially paper over. Spatial only, not temporal - a genuinely aliasing
// highlight still changes frame to frame, just with cleaner edges each time;
// fixing that needs a follow-on TAA pass, not this one.

#pragma once

#include <Common/XenCommon.hpp>

#include "RenderDevice.hpp"

namespace Xen {
    namespace PAK {
        class AssetRegistry;
    }

    class FXAA {
    public:
        struct Settings {
            bool Enabled {true};
        };

        FXAA() = default;
        ~FXAA();

        FXAA(const FXAA&)            = delete;
        FXAA& operator=(const FXAA&) = delete;

        /// @brief Builds the pipeline from the engine shader pak. TargetFormat
        /// is the format Render's Source will always be (a Viewport's color
        /// format - BGRA8_UNORM for the standalone-game presentation path).
        bool Initialize(RHI::IRenderDevice& Device, const PAK::AssetRegistry& Assets, RHI::Format TargetFormat);
        void Shutdown();

        NODISCARD bool IsInitialized() const { return _Device != nullptr; }

        /// @brief Anti-aliases Source - the fully composited frame, 2D and
        /// 3D content alike, whatever a Viewport's color target holds by the
        /// time Game::TickFrame reaches this - into a private scratch
        /// texture, which is what the caller should actually present
        /// (IRenderDevice::CopyToSwapChain) instead of Source. Never writes
        /// into Source itself: reading and writing the same texture in one
        /// pass isn't possible here (see CommandBuffer::CopyTexture's
        /// comment), and there'd be nothing to gain from copying the result
        /// back into Source's own texture when the caller can just present
        /// the scratch directly. Records and submits its own command buffer.
        /// Returns Source unchanged if Settings_.Enabled is off or the
        /// shader failed to load - Shutdown()'s job, not the caller's, to
        /// have noticed that already.
        NODISCARD RHI::TextureHandle Render(RHI::TextureHandle Source, u32 Width, u32 Height, const Settings& Settings_);

    private:
        void EnsureScratch(u32 Width, u32 Height);

        RHI::IRenderDevice* _Device {nullptr};

        RHI::LayoutHandle _Layout {};
        RHI::PipelineHandle _Pipeline {};
        RHI::SamplerHandle _Sampler {};
        RHI::CommandBuffer _Commands;

        RHI::Format _TargetFormat {RHI::Format::BGRA8_UNORM};
        RHI::TextureHandle _Scratch {};
        u32 _ScratchWidth {0};
        u32 _ScratchHeight {0};
    };
}  // namespace Xen
