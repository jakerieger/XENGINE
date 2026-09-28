//
// Created by Jake Rieger on 9/23/2026.
//
// Temporal anti-aliasing: accumulates a jittered sequence of sub-pixel-
// offset frames into a running history buffer, reprojected each frame via
// per-pixel motion vectors (see PBR.hlsl/Sky.hlsl's PSOutput.Velocity) so a
// static scene converges toward a supersampled image and a moving one still
// tracks correctly. The complete fix for both geometric AND shading
// aliasing FXAA (spatial-only) can't reach on its own - see FXAA.hpp's own
// doc comment.
//
// GetJitterOffset supplies the sub-pixel offset MeshRenderer injects into
// the camera's projection matrix before rasterizing a frame (Halton(2,3),
// the standard low-discrepancy TAA jitter sequence, cycled over 8 samples);
// Resolve is the actual temporal accumulation pass, reading this frame's
// jittered color plus motion vectors and blending against the reprojected
// history with a 3x3 neighborhood clamp to bound ghosting.

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Math.hpp>

#include "RenderDevice.hpp"

namespace Xen {
    namespace PAK {
        class AssetRegistry;
    }

    class TAA {
    public:
        struct Settings {
            bool Enabled {true};

            /// Fraction of this frame's raw color blended into the running
            /// history each frame - lower is more temporally stable (less
            /// flicker) but ghosts longer after a disocclusion reveals new
            /// content; higher converges faster but keeps less of the
            /// accumulated supersampling. 0.1 is the standard TAA starting
            /// point (roughly a 10-frame convergence window).
            f32 BlendFactor {0.1f};
        };

        TAA() = default;
        ~TAA();

        TAA(const TAA&)            = delete;
        TAA& operator=(const TAA&) = delete;

        bool Initialize(RHI::IRenderDevice& Device, const PAK::AssetRegistry& Assets, RHI::Format ColorFormat);
        void Shutdown();

        NODISCARD bool IsInitialized() const { return _Device != nullptr; }

        /// @brief This frame's sub-pixel jitter offset, in NDC units (add
        /// directly to a projection matrix's third row - see
        /// MeshRenderer::Render) - Halton(2,3), the standard low-discrepancy
        /// TAA sequence, cycled over 8 samples. Advances the sequence every
        /// call, so only call this once per frame, and only when actually
        /// about to jitter (an unresolved jittered frame - TAA disabled or
        /// unavailable - would just look like a wobbling, half-antialiased
        /// mess with nothing to accumulate it away).
        NODISCARD Float2 GetJitterOffset(u32 Width, u32 Height);

        /// @brief Temporally resolves SceneColor (this frame's jittered,
        /// linear-HDR render) against MotionVectors (see PBR.hlsl/Sky.hlsl)
        /// and the running history into a new, antialiased color - the
        /// texture PostProcess should read in SceneColor's place. Records
        /// into Commands rather than submitting its own buffer, same
        /// convention as SSAO::Render. Returns SceneColor unchanged if
        /// disabled/uninitialized, so a caller can always feed the result
        /// straight into PostProcess either way.
        NODISCARD RHI::TextureHandle Resolve(RHI::CommandBuffer& Commands,
                                             RHI::TextureHandle SceneColor,
                                             RHI::TextureHandle MotionVectors,
                                             u32 Width,
                                             u32 Height,
                                             const Settings& Settings_);

    private:
        void EnsureHistoryTargets(u32 Width, u32 Height);

        RHI::IRenderDevice* _Device {nullptr};
        RHI::Format _ColorFormat {RHI::Format::RGBA16_FLOAT};

        RHI::LayoutHandle _Layout {};
        RHI::PipelineHandle _Pipeline {};  // TAAResolve.hlsl

        // Linear, clamped: History needs bilinear filtering for its
        // reprojected (sub-texel) sample; SceneColor/MotionVectors are read
        // via exact texel Load in the shader instead (a 3x3 neighborhood
        // clamp wants the real, unfiltered texel values), so this sampler
        // is simply unused for those two binds - harmless, and avoids
        // creating a second sampler object for no real benefit.
        RHI::SamplerHandle _Sampler {};

        // Ping-ponged like PostProcess's _AdaptedLuminance: a texture can't
        // be both this pass's own render target and its own SRV input, so
        // History[Next] is written while History[Prev] is read, and the
        // index flips each frame.
        RHI::TextureHandle _History[2] {};
        u32 _HistoryIndex {0};
        bool _HistoryPrimed {false};  // false until a first resolve has actually run at this resolution
        u32 _Width {0};
        u32 _Height {0};

        u32 _JitterIndex {0};
    };
}  // namespace Xen
