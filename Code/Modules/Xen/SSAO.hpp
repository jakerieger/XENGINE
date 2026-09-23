//
// Created by Jake Rieger on 9/23/2026.
//
// Screen-space ambient occlusion. Two fullscreen-triangle passes
// (Code/Shaders/SSAO.hlsl, SSAOBlur.hlsl), reading a camera-space depth
// prepass MeshRenderer renders before this - see MeshRenderer.cpp's own
// comment for why a forward renderer needs that prepass at all (SSAO has to
// exist before PBR.hlsl shades a single pixel, since the result is
// multiplied into the ambient term there, but a forward pass's own depth
// isn't finished until shading is - the classic chicken-and-egg a deferred
// renderer's G-buffer sidesteps for free).

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Math.hpp>

#include "RenderDevice.hpp"

namespace Xen {
    namespace PAK {
        class AssetRegistry;
    }

    class SSAO {
    public:
        struct Settings {
            bool Enabled {true};

            /// World-space radius of the hemisphere each pixel samples -
            /// roughly "how far away can something still occlude me".
            f32 Radius {0.5f};

            /// pow(AO, Power) after averaging - 1 is the raw physical
            /// result; higher darkens and sharpens the effect, matching the
            /// same knob most engines expose (Unreal, Unity's own SSAO).
            f32 Power {1.5f};

            /// World-space bias subtracted from the occluder-distance
            /// comparison, so a sample doesn't register its own surface as
            /// occluding itself from floating-point/reconstruction error -
            /// the same acne-fighting idea as DirectionalLightComponent's
            /// shadow bias, for the same underlying reason.
            f32 Bias {0.03f};
        };

        SSAO() = default;
        ~SSAO();

        SSAO(const SSAO&)            = delete;
        SSAO& operator=(const SSAO&) = delete;

        bool Initialize(RHI::IRenderDevice& Device, const PAK::AssetRegistry& Assets);
        void Shutdown();

        NODISCARD bool IsInitialized() const { return _Device != nullptr; }

        /// @brief Computes ambient occlusion from Depth (MeshRenderer's
        /// camera-space depth prepass, already fully rendered) into a
        /// private, blurred R8_UNORM texture and returns it. Records into
        /// Commands rather than submitting its own buffer - called from
        /// inside MeshRenderer's own command buffer, after the depth
        /// prepass and before the main color pass begins (which is what
        /// actually reads the result). Returns an invalid handle if
        /// Settings_.Enabled is off or the shaders failed to load; the
        /// caller picks its own "no occlusion" fallback (MeshRenderer's own
        /// white placeholder texture), the same convention PostProcess's
        /// bloom slot uses.
        NODISCARD RHI::TextureHandle Render(RHI::CommandBuffer& Commands,
                                            RHI::TextureHandle Depth,
                                            const Float4x4& ViewProjection,
                                            const Float4x4& InvViewProjection,
                                            const Float3& CameraPosition,
                                            u32 Width,
                                            u32 Height,
                                            const Settings& Settings_);

    private:
        void EnsureTargets(u32 Width, u32 Height);

        RHI::IRenderDevice* _Device {nullptr};

        RHI::LayoutHandle _Layout {};
        RHI::PipelineHandle _Pipeline {};  // SSAO.hlsl

        RHI::LayoutHandle _BlurLayout {};
        RHI::PipelineHandle _BlurPipeline {};  // SSAOBlur.hlsl

        // Point, clamped: depth must never be bilinearly blended across a
        // texel edge (that averages two physically unrelated surfaces'
        // depths into a meaningless value), and the blur pass does its own
        // explicit box average, so it doesn't need linear filtering either.
        RHI::SamplerHandle _Sampler {};

        RHI::TextureHandle _RawTarget {};
        RHI::TextureHandle _BlurredTarget {};
        u32 _Width {0};
        u32 _Height {0};
    };
}  // namespace Xen
