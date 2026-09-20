//
// Created by Jake Rieger on 9/19/2026.
//
// Turns a raw equirectangular HDR environment map into the two CUBE maps
// PBR.hlsl and Sky.hlsl actually sample:
//
//   Prefiltered - a cube whose mip chain is the source convolved with the GGX
//                 specular lobe, one roughness per mip (Common.hlsli's
//                 RoughnessForMip), so a glossy surface reads a blurry
//                 reflection with a single sample instead of integrating the
//                 hemisphere per pixel. Mip 0 is a mirror: the source itself,
//                 which is also what the sky is drawn from.
//   Irradiance  - a small cube of the source convolved with a cosine lobe:
//                 whole-hemisphere diffuse lighting, one sample per pixel.
//
// Cube maps rather than equirect: texel density is uniform (no pole
// pinching), filtering is seamless across faces in hardware, and lookup is
// the direction itself - no atan2/asin per sample.
//
// Both are baked on the GPU by full-screen pixel-shader passes (see
// PrefilterEnvironment.hlsl / IrradianceConvolve.hlsl), one draw per (mip,
// face), each into that face's own render-target view.

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
        /// the passes filter by sample density) into Out. SourceWidth sizes
        /// the cube faces (a quarter of it, power of two, capped).
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
