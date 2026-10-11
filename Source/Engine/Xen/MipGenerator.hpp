//
// Created by Jake Rieger on 9/22/2026.
//
// Fills in an ordinary 2D texture's mip chain on the GPU after its mip 0 has
// been uploaded - the missing piece behind TextureCache::Config::GenerateMips
// (see GenerateMips.hlsl). Every step is a fullscreen-triangle box downsample
// into the next mip's own render-target view, the same "render into one mip
// of a texture that already has others" pattern EnvironmentBaker and
// PostProcess's bloom chain already use - no compute shaders anywhere in this
// engine yet.
//
// Unlike an HDR environment map (whose mip pyramid is built on the CPU
// alongside its decode - see TextureCache::DecodeImage/MipTail, needed there
// because EnvironmentBaker samples it by density before any GPU texture
// exists), an ordinary material/sprite texture's mips only ever matter once
// it's a real GPU resource, so generating them on the GPU right after upload
// is both simpler and cheaper than doing the same box-filtering on a worker
// thread.

#pragma once

#include <Common/XenCommon.hpp>

#include "RenderDevice.hpp"

namespace Xen {
    namespace PAK {
        class AssetRegistry;
    }

    class MipGenerator {
    public:
        MipGenerator() = default;
        ~MipGenerator();

        MipGenerator(const MipGenerator&)            = delete;
        MipGenerator& operator=(const MipGenerator&) = delete;

        /// @brief Builds the downsample pipelines from the engine shader pak
        /// - one for RGBA8_UNORM targets, one for RGBA8_SRGB (a D3D12
        /// pipeline's render-target format is fixed at creation, and a
        /// texture may be either - see TextureCache::UploadEntry). Optional:
        /// without the shader, Generate becomes a no-op and a texture that
        /// asked for mips keeps only its mip 0 (TextureCache falls back to
        /// creating a single-mip texture in that case, so this never leaves
        /// a mip chain half-populated).
        bool Initialize(RHI::IRenderDevice& Device, const PAK::AssetRegistry& Assets);
        void Shutdown();

        NODISCARD bool IsInitialized() const { return _Device != nullptr; }

        /// @brief Fills mips 1..Levels-1 of Texture from its already-uploaded
        /// mip 0. Texture must have been created with ColorTarget|Sampled
        /// usage and exactly Levels mip levels (TextureCache::UploadEntry
        /// arranges both). Synchronous (SubmitAndWait) - Texture is fully
        /// populated by the time this returns, safe to sample from the very
        /// next draw call regardless of where in a frame this is called
        /// from, matching how IRenderDevice::UploadTexture itself already
        /// behaves. A no-op if Levels <= 1 or this isn't initialized.
        void Generate(RHI::TextureHandle Texture, u32 Width, u32 Height, u32 Levels, bool Srgb);

    private:
        void EnsureScratch(u32 Width, u32 Height, RHI::Format Format);

        RHI::IRenderDevice* _Device {nullptr};

        RHI::LayoutHandle _Layout {};
        RHI::PipelineHandle _UnormPipeline {};
        RHI::PipelineHandle _SrgbPipeline {};
        RHI::SamplerHandle _Sampler {};
        RHI::CommandBuffer _Commands;

        // Every downsample pass reads the mip it's about to refine from this
        // (single-mip, resized/reformatted on demand) scratch texture rather
        // than from Texture itself, even though Texture already holds that
        // exact data one mip up - see CommandBuffer::CopyTexture's comment
        // for why sampling Texture while also rendering into a different mip
        // of it has no correct state to put it in, under how this backend
        // tracks a texture's GPU resource state. One shared scratch, reused
        // (and grown/reformatted as needed) across every texture this
        // generates mips for, not recreated per call.
        RHI::TextureHandle _Scratch {};
        u32 _ScratchWidth {0};
        u32 _ScratchHeight {0};
        RHI::Format _ScratchFormat {RHI::Format::Unknown};
    };
}  // namespace Xen
