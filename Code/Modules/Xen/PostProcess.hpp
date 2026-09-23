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

#include <vector>

namespace Xen {
    namespace PAK {
        class AssetRegistry;
    }

    class PostProcess {
    public:
        struct Settings {
            /// Multiplies the scene's linear color before tonemapping,
            /// *while AutoExposureEnabled is off* - the same knob a
            /// camera's manual exposure dial is. 1 is neutral, but "neutral"
            /// is only ever right for the one lighting setup it was tuned
            /// against: an HDRI's own radiance values aren't normalized to
            /// any particular range (studio.hdr's walls meter around 0.7,
            /// its light fixture past 20), so a fixed Exposure has to be
            /// re-tuned by hand for every environment. AutoExposureEnabled
            /// exists to not need that.
            f32 Exposure {1.0f};

            /// @brief Meters the scene's own average brightness every frame
            /// (see PostProcess.cpp's luminance chain, run before the
            /// composite pass) and derives the exposure multiplier from
            /// that instead of using Exposure - what a camera's auto
            /// exposure, or an eye's pupil, does. On by default. Turn it
            /// off to fall back to a fixed Exposure (e.g. for a cutscene
            /// camera that shouldn't visibly re-expose, or to A/B against
            /// a hand-tuned value).
            bool AutoExposureEnabled {true};

            /// The metered average luminance is scaled so it lands here -
            /// 0.18, "18% middle gray", is the standard photographic
            /// convention. Raise it for a brighter auto-exposed image,
            /// lower for darker.
            f32 AutoExposureKey {0.18f};

            /// Clamps applied to the *metered* luminance before it's turned
            /// into an exposure value - keeps one bright window, or a
            /// momentarily pitch-black frame, from swinging exposure to an
            /// extreme. Same linear radiance units as the scene itself (an
            /// outdoor HDRI's sky can meter well above 1).
            f32 AutoExposureMinLuminance {0.03f};
            f32 AutoExposureMaxLuminance {16.0f};

            /// Exponential time constants (seconds) auto exposure's
            /// eye-adaptation takes to catch up to a brightness change -
            /// separate knobs because a scene getting brighter should pull
            /// exposure down faster than a scene getting darker should
            /// bring it back up (a real pupil constricts quicker than it
            /// dilates), so a light switching on doesn't leave the frame
            /// blown out for a moment first.
            f32 AutoExposureAdaptUpSeconds {0.3f};
            f32 AutoExposureAdaptDownSeconds {1.0f};

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

        /// @brief Records auto-exposure metering (if enabled and its shaders
        /// loaded), bloom (if enabled and its shaders loaded) and the
        /// exposure/tonemap composite into Commands - call after SceneColor's
        /// own render pass has ended, before Commands is submitted; this
        /// records more passes into the same buffer rather than submitting
        /// its own. SceneColor must be RGBA16F, SceneWidth/SceneHeight its
        /// exact size, and Target the real destination (typically a
        /// Viewport's color target) - see the class comment for how the two
        /// combine. DeltaTime drives auto exposure's eye-adaptation ramp;
        /// ignored entirely when Settings_.AutoExposureEnabled is off.
        void Render(RHI::CommandBuffer& Commands,
                   RHI::TextureHandle SceneColor,
                   u32 SceneWidth,
                   u32 SceneHeight,
                   RHI::TextureHandle Target,
                   f32 DeltaTime,
                   const Settings& Settings_);

    private:
        void EnsureBloomChain(u32 SceneWidth, u32 SceneHeight);
        void EnsureLuminanceChain(u32 SceneWidth, u32 SceneHeight);

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

        // Every downsample/upsample pass past the first reads a mip of
        // _BloomChain while a *different* mip of that same texture is the
        // pass's own render target - reading it through a same-sized scratch
        // copy instead of _BloomChain directly avoids that (see
        // CommandBuffer::CopyTexture's comment for why). One persistent
        // scratch texture *per level* (_Scratches[Level] sized to match
        // _BloomChain's own mip Level, recreated alongside it in
        // EnsureBloomChain - never mid-frame) rather than one reused,
        // resized-per-level scratch: this runs every frame, so unlike
        // MipGenerator (a one-shot load-time cost that can afford to wait
        // out the GPU between levels) there's no submitting between levels
        // to make a destroy-and-recreate-mid-chain safe - see
        // MipGenerator::Generate's own comment for what goes wrong without
        // that. A stable texture per level sidesteps the problem instead of
        // paying for it.
        std::vector<RHI::TextureHandle> _Scratches;

        // --- Auto exposure ---------------------------------------------
        //
        // Three passes, run before the composite pass (see PostProcess.cpp):
        //   Measure - box-downsamples SceneColor to half its resolution
        //             while converting to log2 luminance (LuminanceMeasure.hlsl).
        //   Reduce  - halves that, log-averaged, down to exactly 1x1
        //             (LuminanceReduce.hlsl, repeated).
        //   Adapt   - blends the previous frame's adapted luminance toward
        //             this frame's measured 1x1 value (LuminanceAdapt.hlsl),
        //             so a brightness change ramps in over
        //             Settings::AutoExposureAdaptUp/DownSeconds instead of
        //             snapping.
        // The composite pass (PostProcessComposite.hlsl) then reads the
        // adapted value back and derives Exposure from it: Key / AvgLuminance.
        RHI::LayoutHandle _LumLayout {};  // shared by Measure + Reduce (b0 + t0/s0 - same shape as _BloomLayout)
        RHI::PipelineHandle _LumMeasurePipeline {};
        RHI::PipelineHandle _LumReducePipeline {};

        RHI::LayoutHandle _LumAdaptLayout {};  // b0 + t0/s0 (previous adapted) + t1/s1 (this frame's measured)
        RHI::PipelineHandle _LumAdaptPipeline {};

        // The metering chain: level 0 is SceneWidth/2 x SceneHeight/2,
        // recreated (see EnsureLuminanceChain) if that resolution changes;
        // every level after halves the one before, down to exactly 1x1 (no
        // early stop the way _BloomChain's MaxBloomLevels allows itself -
        // LuminanceAdapt.hlsl needs a real single-texel average). One
        // independent texture per level, not mips of one texture, for the
        // same reason _Scratches exists: level k reads level k-1 through a
        // genuinely separate GPU resource, so unlike _BloomChain there's
        // nothing here for CommandBuffer::CopyTexture to route around.
        std::vector<RHI::TextureHandle> _LumChain;
        u32 _LumBaseWidth {0};
        u32 _LumBaseHeight {0};

        // The eye-adaptation-smoothed average scene luminance: one
        // persistent 1x1 texture per frame parity, ping-ponged each frame
        // (Adapt reads _AdaptedLuminance[_AdaptedLuminanceIndex], writes the
        // other slot, then flips the index) rather than read-and-written in
        // place - a texture can't be both a pass's render target and its
        // own SRV input (see CommandBuffer::CopyTexture's comment). Created
        // once in Initialize at 1x1 and never touched by
        // EnsureLuminanceChain/resize.
        RHI::TextureHandle _AdaptedLuminance[2] {};
        u32 _AdaptedLuminanceIndex {0};
        bool _AdaptedLuminancePrimed {false};  // false until a first value has actually been written
    };
}  // namespace Xen
