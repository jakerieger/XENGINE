//
// Created by Jake Rieger on 9/22/2026.
//
// The last step of a 3D frame: turns MeshRenderer's linear HDR scene render
// into what actually lands on screen. Two effects, run back to back:
//
//   Bloom     - a "physically based" mip-chain blur (Call of Duty: Advanced
//               Warfare's SIGGRAPH 2014 technique): a soft-thresholded bright
//               pass seeds a chain of progressively half-sized mips
//               (BloomDownsample.hlsl), which are then blended back up into
//               one another with a wide tent filter (BloomUpsample.hlsl),
//               leaving mip 0 a multi-scale glow around anything bright
//               enough to cross the threshold.
//   Composite - exposure, then ACES tonemap + gamma encode
//               (Include/Tonemap.hlsli), blended over whatever was already
//               in Target using the scene render's own alpha (1 where
//               MeshRenderer actually drew, 0 elsewhere), so content from
//               outside the 3D pass - 2D sprites - is left alone.
//
// Every pass here is a fullscreen triangle (Include/Fullscreen.hlsli), same
// as EnvironmentBaker's bake passes - no compute shaders anywhere in this
// engine yet.

#pragma once

#include <Common/XenCommon.hpp>

#include "RenderDevice.hpp"

namespace Xen {
    namespace PAK {
        class AssetRegistry;
    }

    class PostProcess {
    public:
        struct Settings {
            /// Multiplies the scene's linear color before tonemapping. 1 is
            /// neutral; raise it to brighten a dim scene, lower it to pull a
            /// bright one back down - the same knob a camera's exposure dial
            /// is.
            f32 Exposure {1.0f};

            bool BloomEnabled {true};

            /// A pixel's brightest channel must exceed this to seed the
            /// bloom chain at all.
            f32 BloomThreshold {1.0f};

            /// 0..1: how gradually the threshold engages, rather than
            /// cutting hard at it - 0 is a hard cutoff, wider values fade
            /// bloom in starting below Threshold. Unity's soft-knee curve.
            f32 BloomSoftKnee {0.5f};

            /// How much of the blurred result is added back into the scene.
            /// Small: the chain is a full-strength blur of (thresholded)
            /// highlights, not a dim one.
            f32 BloomIntensity {0.04f};
        };

        PostProcess() = default;
        ~PostProcess();

        PostProcess(const PostProcess&)            = delete;
        PostProcess& operator=(const PostProcess&) = delete;

        /// @brief Builds the bloom and composite pipelines from the engine
        /// shader pak. TargetFormat is the format Render's Target will
        /// always be (a Viewport's color format), matching how MeshRenderer
        /// fixes its own pipeline's format at Initialize time.
        bool Initialize(RHI::IRenderDevice& Device, const PAK::AssetRegistry& Assets, RHI::Format TargetFormat);
        void Shutdown();

        NODISCARD bool IsInitialized() const { return _Device != nullptr; }

        /// @brief Records bloom (if enabled and its shaders loaded) and the
        /// exposure/tonemap composite into Commands - call after SceneColor's
        /// own render pass has ended, before Commands is submitted; this
        /// records more passes into the same buffer rather than submitting
        /// its own. SceneColor must be RGBA16F, SceneWidth/SceneHeight its
        /// exact size, and Target the real destination (typically a
        /// Viewport's color target) - see the class comment for how the two
        /// combine.
        void Render(RHI::CommandBuffer& Commands,
                   RHI::TextureHandle SceneColor,
                   u32 SceneWidth,
                   u32 SceneHeight,
                   RHI::TextureHandle Target,
                   const Settings& Settings_);

    private:
        void EnsureBloomChain(u32 SceneWidth, u32 SceneHeight);

        RHI::IRenderDevice* _Device {nullptr};

        RHI::LayoutHandle _BloomLayout {};
        RHI::PipelineHandle _DownsamplePipeline {};
        RHI::PipelineHandle _UpsamplePipeline {};

        RHI::LayoutHandle _CompositeLayout {};
        RHI::PipelineHandle _CompositePipeline {};

        RHI::SamplerHandle _Sampler {};

        // The bloom mip chain: one texture, half the scene's resolution at
        // mip 0, recreated (see EnsureBloomChain) if that resolution
        // changes. Downsample writes mip k from mip k-1 (mip 0 reads the
        // scene color instead); upsample additively blends mip k back into
        // mip k-1, smallest level first, so mip 0 ends up the final result.
        RHI::TextureHandle _BloomChain {};
        u32 _BloomWidth {0};
        u32 _BloomHeight {0};
        u32 _BloomLevels {0};
    };
}  // namespace Xen
