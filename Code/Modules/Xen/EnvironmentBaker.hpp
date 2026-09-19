//
// Created by Jake Rieger on 9/19/2026.
//
// Turns a raw equirectangular HDR environment map into the two maps PBR.hlsl
// actually samples for image-based lighting:
//
//   Prefiltered - a mip chain where each mip is the source convolved with the
//                 GGX specular lobe for one roughness (mip / (mips - 1)), so a
//                 glossy surface reads a blurry reflection with a single
//                 sample instead of integrating the hemisphere per pixel.
//   Irradiance  - a small map of the source convolved with a cosine lobe:
//                 whole-hemisphere diffuse lighting, one sample per pixel.
//
// Both are baked on the GPU by full-screen pixel-shader passes (see
// PrefilterEnvironment.hlsl / IrradianceConvolve.hlsl), one draw per mip.

#pragma once

#include <Common/XenCommon.hpp>

#include "RenderDevice.hpp"

namespace Xen {
    namespace PAK {
        class AssetRegistry;
    }

    class EnvironmentBaker {
    public:
        struct Result {
            RHI::TextureHandle Prefiltered {};
            RHI::TextureHandle Irradiance {};
        };

        EnvironmentBaker() = default;
        ~EnvironmentBaker();

        EnvironmentBaker(const EnvironmentBaker&)            = delete;
        EnvironmentBaker& operator=(const EnvironmentBaker&) = delete;

        /// @brief Builds the bake pipelines from the engine shader pak.
        /// Returns false (leaving the baker unusable) if a shader asset is
        /// missing - the caller falls back to its placeholder environment.
        bool Initialize(RHI::IRenderDevice& Device, const PAK::AssetRegistry& Assets);
        void Shutdown();

        NODISCARD bool IsInitialized() const { return _Device != nullptr; }

        /// @brief Bakes Source (an RGBA16F equirect map with a full mip
        /// pyramid - see TextureCache's HDR path; its lower mips are what
        /// the passes filter by sample density) into Out.
        ///
        /// Records into the device's CURRENT frame via Submit, so call it
        /// between BeginFrame and EndFrame; the GPU orders it before whatever
        /// the caller records next. The caller owns Out's textures and must
        /// DestroyTexture them.
        bool Bake(RHI::TextureHandle Source, u32 SourceWidth, Result& Out);

    private:
        RHI::IRenderDevice* _Device {nullptr};

        RHI::LayoutHandle _Layout {};
        RHI::PipelineHandle _PrefilterPipeline {};
        RHI::PipelineHandle _IrradiancePipeline {};
        RHI::SamplerHandle _Sampler {};
        RHI::CommandBuffer _Commands;
    };
}  // namespace Xen
