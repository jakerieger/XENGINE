//
// Created by Jake Rieger on 9/22/2026.
//

#include "MipGenerator.hpp"

#include <XenPAK/AssetRegistry.hpp>
#include <XenPAK/AssetBuffer.hpp>

namespace Xen {
    namespace {
        // Matches GenerateMips.hlsl's cbuffer exactly.
        struct GenerateMipsParams {
            f32 TexelSize[4];
            f32 SourceMip[4];
        };

        RHI::ShaderHandle LoadShader(RHI::IRenderDevice& Device,
                                     const PAK::AssetRegistry& Assets,
                                     const AssetID Asset,
                                     const RHI::ShaderStage Stage,
                                     const char* DebugName) {
            if (!Assets.Contains(Asset)) return {};

            const PAK::AssetBuffer Source = Assets.Load(Asset);

            RHI::ShaderDesc Desc;
            Desc.Stage      = Stage;
            Desc.SourceType = RHI::ShaderSourceType::DXIL;
            Desc.Code       = Source.Data();
            Desc.CodeSize   = Source.Size();
            Desc.DebugName  = DebugName;
            return Device.CreateShader(Desc);
        }
    }  // namespace

    MipGenerator::~MipGenerator() {
        Shutdown();
    }

    bool MipGenerator::Initialize(RHI::IRenderDevice& Device, const PAK::AssetRegistry& Assets) {
        _Device = &Device;

        const RHI::ShaderHandle Vertex = LoadShader(
          Device, Assets, ASSET("xen.shader.generatemips.vs"), RHI::ShaderStage::Vertex, "XEN.Shaders.GenerateMips.vs");
        const RHI::ShaderHandle Fragment = LoadShader(Device,
                                                      Assets,
                                                      ASSET("xen.shader.generatemips.ps"),
                                                      RHI::ShaderStage::Fragment,
                                                      "XEN.Shaders.GenerateMips.ps");

        RHI::PipelineLayoutDesc LayoutDesc;
        LayoutDesc.Binding(0, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment);
        LayoutDesc.DebugName = "XEN.Shaders.GenerateMips";
        _Layout              = Device.CreatePipelineLayout(LayoutDesc);

        const auto MakePipeline = [&](const RHI::Format ColorFormat, const char* Name) {
            if (!Vertex.IsValid() || !Fragment.IsValid() || !_Layout.IsValid()) return RHI::PipelineHandle {};

            RHI::GraphicsPipelineDesc Desc;
            Desc.VertexShader         = Vertex;
            Desc.FragmentShader       = Fragment;
            Desc.PipelineLayout       = _Layout;
            Desc.Topology             = RHI::PrimitiveTopology::TriangleList;
            Desc.ColorAttachmentCount = 1;
            Desc.ColorFormats[0]      = ColorFormat;
            Desc.DebugName            = Name;
            return Device.CreateGraphicsPipeline(Desc);
        };

        // Two pipelines, not one: a D3D12 pipeline's render-target format is
        // fixed at creation, and the texture being mipped may be either
        // RGBA8_UNORM (a data map) or RGBA8_SRGB (a color map) - see
        // TextureCache::UploadEntry. Generate picks the matching one.
        _UnormPipeline = MakePipeline(RHI::Format::RGBA8_UNORM, "XEN.Shaders.GenerateMips.Unorm");
        _SrgbPipeline  = MakePipeline(RHI::Format::RGBA8_SRGB, "XEN.Shaders.GenerateMips.Srgb");

        if (Vertex.IsValid()) Device.DestroyShader(Vertex);
        if (Fragment.IsValid()) Device.DestroyShader(Fragment);

        // Wraps, not clamps: these are ordinarily tiled material/sprite
        // textures (see MeshRenderer's own material sampler), and a mip
        // built with edge-clamping would show a seam once something actually
        // tiles it. MipFilter is irrelevant - every sample picks its mip
        // explicitly (SampleLevel).
        RHI::SamplerDesc SamplerDesc;
        SamplerDesc.MipFilter = RHI::MipMode::None;
        SamplerDesc.DebugName = "XEN.MipGenerator";
        _Sampler              = Device.CreateSampler(SamplerDesc);

        if (!_UnormPipeline.IsValid() || !_SrgbPipeline.IsValid() || !_Sampler.IsValid()) {
            LOG_WARN("mip generation shader not found or failed to compile - textures will have no generated mips");
            Shutdown();
            return false;
        }
        return true;
    }

    void MipGenerator::Shutdown() {
        if (!_Device) return;

        if (_UnormPipeline.IsValid()) _Device->DestroyPipeline(_UnormPipeline);
        if (_SrgbPipeline.IsValid()) _Device->DestroyPipeline(_SrgbPipeline);
        if (_Layout.IsValid()) _Device->DestroyPipelineLayout(_Layout);
        if (_Sampler.IsValid()) _Device->DestroySampler(_Sampler);
        if (_Scratch.IsValid()) _Device->DestroyTexture(_Scratch);

        _UnormPipeline = {};
        _SrgbPipeline  = {};
        _Layout        = {};
        _Sampler       = {};
        _Scratch       = {};
        _ScratchWidth = _ScratchHeight = 0;
        _ScratchFormat = RHI::Format::Unknown;
        _Device        = nullptr;
    }

    void MipGenerator::EnsureScratch(const u32 Width, const u32 Height, const RHI::Format Format) {
        if (_Scratch.IsValid() && _ScratchWidth == Width && _ScratchHeight == Height && _ScratchFormat == Format) {
            return;
        }

        if (_Scratch.IsValid()) _Device->DestroyTexture(_Scratch);

        RHI::TextureDesc Desc;
        Desc.Width     = Width;
        Desc.Height    = Height;
        Desc.Fmt       = Format;
        Desc.Usage     = RHI::TextureUsage::ColorTarget | RHI::TextureUsage::Sampled | RHI::TextureUsage::CopyDst;
        Desc.DebugName = "XEN.MipGenerator.Scratch";

        _Scratch       = _Device->CreateTexture(Desc);
        _ScratchWidth  = Width;
        _ScratchHeight = Height;
        _ScratchFormat = Format;
    }

    void MipGenerator::Generate(const RHI::TextureHandle Texture,
                                const u32 Width,
                                const u32 Height,
                                const u32 Levels,
                                const bool Srgb) {
        if (!_Device || Levels <= 1) return;

        const RHI::PipelineHandle Pipeline = Srgb ? _SrgbPipeline : _UnormPipeline;
        if (!Pipeline.IsValid()) return;

        const RHI::Format ScratchFormat = Srgb ? RHI::Format::RGBA8_SRGB : RHI::Format::RGBA8_UNORM;

        // One Submit per level, not one for the whole chain: EnsureScratch
        // below destroys and recreates _Scratch as each level's size
        // shrinks, and a texture destroyed while a command buffer that
        // still references its handle hasn't been submitted yet is a real
        // bug, not just a resource leak - handle IDs get freed and reused
        // immediately (see TextureCache::UploadEntry's own destroy/recreate
        // for a texture that IS safe this way, because nothing references
        // the old one afterward). Waiting out each level here means
        // EnsureScratch never touches a texture a still-pending command
        // buffer is about to read. This runs once per texture load, not
        // per frame, so the extra GPU round-trips cost nothing that
        // matters next to the PNG decode that already happened to get here.
        u32 SourceWidth = Width, SourceHeight = Height;
        for (u32 Level = 1; Level < Levels; ++Level) {
            EnsureScratch(SourceWidth, SourceHeight, ScratchFormat);

            _Commands.Reset();
            _Commands.PushDebugGroup("Generate mip");
            _Commands.BindPipeline(Pipeline);

            // Texture's own mip (Level - 1) already holds exactly the data
            // this pass needs to read - copied out to a separate texture
            // first because Texture itself is about to be this same pass's
            // render target (a different mip, but see CopyTexture's comment
            // for why that still matters).
            _Commands.CopyTexture(Texture, Level - 1, _Scratch, 0);

            RHI::RenderPassDesc Pass = RHI::RenderPassDesc::ColorTarget(Texture);
            Pass.ColorAttachments[0].MipLevel = Level;
            Pass.DebugName                    = "Generate mip";
            _Commands.BeginRenderPass(Pass);

            GenerateMipsParams Params {};
            Params.TexelSize[0] = 1.0f / CAST<f32>(SourceWidth);
            Params.TexelSize[1] = 1.0f / CAST<f32>(SourceHeight);
            Params.SourceMip[0] = 0.0f;  // the scratch texture has exactly one mip
            _Commands.BindUniformBuffer(0, _Device->AllocateUniform(Params));
            _Commands.BindTexture(0, _Scratch, _Sampler);
            _Commands.Draw(3);
            _Commands.EndRenderPass();

            _Commands.PopDebugGroup();

            // Synchronous: the caller (TextureCache::UploadEntry) needs
            // Texture fully populated before it returns, the same guarantee
            // UploadTexture's own ExecuteUploadAndWait already makes for mip
            // 0 - a texture adopted by AssetLoader::Pump mid-frame can be
            // drawn with later in that very frame.
            _Device->SubmitAndWait(_Commands);

            SourceWidth  = std::max(Width >> Level, 1u);
            SourceHeight = std::max(Height >> Level, 1u);
        }
    }
}  // namespace Xen
