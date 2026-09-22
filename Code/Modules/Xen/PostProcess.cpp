//
// Created by Jake Rieger on 9/22/2026.
//

#include "PostProcess.hpp"

#include <XenPAK/AssetRegistry.hpp>
#include <XenPAK/AssetBuffer.hpp>

#include <algorithm>

namespace Xen {
    namespace {
        constexpr RHI::Format BloomFormat = RHI::Format::RGBA16_FLOAT;

        // Mips run until a side drops to 8 texels or this many levels,
        // whichever comes first - the same reasoning as EnvironmentBaker's
        // MaxLevels: past a certain point a coarser level changes nothing
        // visible, only cost.
        constexpr u32 MaxBloomLevels = 6;

        // Matches BloomDownsample.hlsl / BloomUpsample.hlsl's cbuffers
        // exactly.
        struct DownsampleParams {
            f32 TexelSize[4];
            f32 Threshold[4];
        };

        struct UpsampleParams {
            f32 TexelSize[4];
            f32 Params2[4];
        };

        // Matches PostProcessComposite.hlsl's cbuffer.
        struct CompositeParams {
            f32 Params[4];
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

    PostProcess::~PostProcess() {
        Shutdown();
    }

    bool PostProcess::Initialize(RHI::IRenderDevice& Device,
                                 const PAK::AssetRegistry& Assets,
                                 const RHI::Format TargetFormat) {
        _Device = &Device;

        const RHI::ShaderHandle DownsampleVs = LoadShader(Device,
                                                          Assets,
                                                          ASSET("xen.shader.bloomdownsample.vs"),
                                                          RHI::ShaderStage::Vertex,
                                                          "XEN.Shaders.BloomDownsample.vs");
        const RHI::ShaderHandle DownsamplePs = LoadShader(Device,
                                                          Assets,
                                                          ASSET("xen.shader.bloomdownsample.ps"),
                                                          RHI::ShaderStage::Fragment,
                                                          "XEN.Shaders.BloomDownsample.ps");
        const RHI::ShaderHandle UpsampleVs = LoadShader(Device,
                                                        Assets,
                                                        ASSET("xen.shader.bloomupsample.vs"),
                                                        RHI::ShaderStage::Vertex,
                                                        "XEN.Shaders.BloomUpsample.vs");
        const RHI::ShaderHandle UpsamplePs = LoadShader(Device,
                                                        Assets,
                                                        ASSET("xen.shader.bloomupsample.ps"),
                                                        RHI::ShaderStage::Fragment,
                                                        "XEN.Shaders.BloomUpsample.ps");

        // Both bloom passes bind the same shape: b0 = params, t0/s0 = the
        // one texture they read (the scene, or the chain's own previous
        // level).
        RHI::PipelineLayoutDesc BloomLayoutDesc;
        BloomLayoutDesc.Binding(0, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment);
        BloomLayoutDesc.DebugName = "XEN.Shaders.Bloom";
        _BloomLayout              = Device.CreatePipelineLayout(BloomLayoutDesc);

        const auto MakeBloomPipeline =
          [&](const RHI::ShaderHandle Vertex, const RHI::ShaderHandle Fragment, const bool Additive, const char* Name) {
              if (!Vertex.IsValid() || !Fragment.IsValid() || !_BloomLayout.IsValid()) return RHI::PipelineHandle {};

              RHI::GraphicsPipelineDesc Desc;
              Desc.VertexShader         = Vertex;
              Desc.FragmentShader       = Fragment;
              Desc.PipelineLayout       = _BloomLayout;
              Desc.Topology             = RHI::PrimitiveTopology::TriangleList;
              Desc.ColorAttachmentCount = 1;
              Desc.ColorFormats[0]      = BloomFormat;
              if (Additive) Desc.Blend.Attachments[0] = RHI::BlendAttachmentState::Additive();
              Desc.DebugName = Name;
              return Device.CreateGraphicsPipeline(Desc);
          };

        _DownsamplePipeline = MakeBloomPipeline(DownsampleVs, DownsamplePs, false, "XEN.Shaders.BloomDownsample");
        _UpsamplePipeline   = MakeBloomPipeline(UpsampleVs, UpsamplePs, true, "XEN.Shaders.BloomUpsample");

        for (const RHI::ShaderHandle Shader : {DownsampleVs, DownsamplePs, UpsampleVs, UpsamplePs}) {
            if (Shader.IsValid()) Device.DestroyShader(Shader);
        }

        const RHI::ShaderHandle CompositeVs = LoadShader(Device,
                                                         Assets,
                                                         ASSET("xen.shader.postprocesscomposite.vs"),
                                                         RHI::ShaderStage::Vertex,
                                                         "XEN.Shaders.PostProcessComposite.vs");
        const RHI::ShaderHandle CompositePs = LoadShader(Device,
                                                         Assets,
                                                         ASSET("xen.shader.postprocesscomposite.ps"),
                                                         RHI::ShaderStage::Fragment,
                                                         "XEN.Shaders.PostProcessComposite.ps");

        RHI::PipelineLayoutDesc CompositeLayoutDesc;
        CompositeLayoutDesc.Binding(0, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment)
          .Binding(1, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Fragment)
          .Binding(1, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment);
        CompositeLayoutDesc.DebugName = "XEN.Shaders.PostProcessComposite";
        _CompositeLayout              = Device.CreatePipelineLayout(CompositeLayoutDesc);

        if (CompositeVs.IsValid() && CompositePs.IsValid() && _CompositeLayout.IsValid()) {
            RHI::GraphicsPipelineDesc Desc;
            Desc.VertexShader                 = CompositeVs;
            Desc.FragmentShader               = CompositePs;
            Desc.PipelineLayout               = _CompositeLayout;
            Desc.Topology                     = RHI::PrimitiveTopology::TriangleList;
            Desc.ColorAttachmentCount         = 1;
            Desc.ColorFormats[0]              = TargetFormat;
            // Non-premultiplied alpha, blended over whatever the target
            // already holds - see the class comment.
            Desc.Blend.Attachments[0]         = RHI::BlendAttachmentState::AlphaBlend();
            Desc.DebugName                    = "XEN.Shaders.PostProcessComposite";
            _CompositePipeline                = Device.CreateGraphicsPipeline(Desc);
        }

        if (CompositeVs.IsValid()) Device.DestroyShader(CompositeVs);
        if (CompositePs.IsValid()) Device.DestroyShader(CompositePs);

        if (!_DownsamplePipeline.IsValid() || !_UpsamplePipeline.IsValid()) {
            LOG_WARN("bloom shaders not found or failed to compile - bloom will be unavailable");
        }
        if (!_CompositePipeline.IsValid()) {
            LOG_ERR("post-process composite pipeline failed - 3D rendering will show nothing (no shader asset?)");
        }

        RHI::SamplerDesc SamplerDesc;
        SamplerDesc.MipFilter   = RHI::MipMode::None;  // every sample picks its mip explicitly (SampleLevel)
        SamplerDesc.AddressU    = RHI::AddressMode::ClampToEdge;
        SamplerDesc.AddressV    = RHI::AddressMode::ClampToEdge;
        SamplerDesc.DebugName   = "XEN.PostProcess";
        _Sampler                = Device.CreateSampler(SamplerDesc);

        if (!_CompositePipeline.IsValid() || !_Sampler.IsValid()) {
            Shutdown();
            return false;
        }
        return true;
    }

    void PostProcess::Shutdown() {
        if (!_Device) return;

        if (_DownsamplePipeline.IsValid()) _Device->DestroyPipeline(_DownsamplePipeline);
        if (_UpsamplePipeline.IsValid()) _Device->DestroyPipeline(_UpsamplePipeline);
        if (_BloomLayout.IsValid()) _Device->DestroyPipelineLayout(_BloomLayout);
        if (_CompositePipeline.IsValid()) _Device->DestroyPipeline(_CompositePipeline);
        if (_CompositeLayout.IsValid()) _Device->DestroyPipelineLayout(_CompositeLayout);
        if (_Sampler.IsValid()) _Device->DestroySampler(_Sampler);
        if (_BloomChain.IsValid()) _Device->DestroyTexture(_BloomChain);

        _DownsamplePipeline = {};
        _UpsamplePipeline   = {};
        _BloomLayout        = {};
        _CompositePipeline  = {};
        _CompositeLayout    = {};
        _Sampler            = {};
        _BloomChain         = {};
        _BloomWidth = _BloomHeight = _BloomLevels = 0;
        _Device                                   = nullptr;
    }

    void PostProcess::EnsureBloomChain(const u32 SceneWidth, const u32 SceneHeight) {
        const u32 Width  = std::max(SceneWidth / 2, 1u);
        const u32 Height = std::max(SceneHeight / 2, 1u);
        if (_BloomChain.IsValid() && _BloomWidth == Width && _BloomHeight == Height) return;

        if (_BloomChain.IsValid()) _Device->DestroyTexture(_BloomChain);

        u32 Levels = 1;
        for (u32 W = Width, H = Height; W > 8 && H > 8 && Levels < MaxBloomLevels; W /= 2, H /= 2) ++Levels;

        RHI::TextureDesc Desc;
        Desc.Width     = Width;
        Desc.Height    = Height;
        Desc.MipLevels = Levels;
        Desc.Fmt       = BloomFormat;
        Desc.Usage     = RHI::TextureUsage::ColorTarget | RHI::TextureUsage::Sampled;
        Desc.DebugName = "XEN.PostProcess.Bloom";

        _BloomChain  = _Device->CreateTexture(Desc);
        _BloomWidth  = Width;
        _BloomHeight = Height;
        _BloomLevels = _BloomChain.IsValid() ? Levels : 0;
    }

    void PostProcess::Render(RHI::CommandBuffer& Commands,
                             const RHI::TextureHandle SceneColor,
                             const u32 SceneWidth,
                             const u32 SceneHeight,
                             const RHI::TextureHandle Target,
                             const Settings& Settings_) {
        if (!_Device || !_CompositePipeline.IsValid()) return;

        const bool WantsBloom = Settings_.BloomEnabled && _DownsamplePipeline.IsValid() && _UpsamplePipeline.IsValid();
        if (WantsBloom) EnsureBloomChain(SceneWidth, SceneHeight);
        const bool UseBloom = WantsBloom && _BloomChain.IsValid();

        if (UseBloom) {
            Commands.PushDebugGroup("Bloom");

            // Downsample: level 0 reads the scene (thresholded), every level
            // after reads the chain's own previous level.
            u32 SourceWidth = SceneWidth, SourceHeight = SceneHeight;
            for (u32 Level = 0; Level < _BloomLevels; ++Level) {
                RHI::RenderPassDesc Pass = RHI::RenderPassDesc::ColorTarget(_BloomChain, 0.0f, 0.0f, 0.0f, 0.0f);
                Pass.ColorAttachments[0].MipLevel = Level;
                Pass.DebugName                    = "Bloom downsample";
                Commands.BeginRenderPass(Pass);
                Commands.BindPipeline(_DownsamplePipeline);

                DownsampleParams Params {};
                Params.TexelSize[0] = 1.0f / CAST<f32>(SourceWidth);
                Params.TexelSize[1] = 1.0f / CAST<f32>(SourceHeight);
                Params.Threshold[0] = Settings_.BloomThreshold;
                Params.Threshold[1] = Settings_.BloomSoftKnee;
                Params.Threshold[2] = Level == 0 ? 1.0f : 0.0f;
                Params.Threshold[3] = Level == 0 ? 0.0f : CAST<f32>(Level - 1);
                Commands.BindUniformBuffer(0, _Device->AllocateUniform(Params));
                Commands.BindTexture(0, Level == 0 ? SceneColor : _BloomChain, _Sampler);
                Commands.Draw(3);
                Commands.EndRenderPass();

                SourceWidth  = std::max(_BloomWidth >> Level, 1u);
                SourceHeight = std::max(_BloomHeight >> Level, 1u);
            }

            // Upsample + additively combine, smallest level first, so each
            // step's destination mip already holds its own downsampled
            // content (Load, not Clear) for the blend to land on.
            for (u32 Level = _BloomLevels - 1; Level > 0; --Level) {
                const u32 DstLevel      = Level - 1;
                const u32 SourceMipWidth  = std::max(_BloomWidth >> Level, 1u);
                const u32 SourceMipHeight = std::max(_BloomHeight >> Level, 1u);

                RHI::RenderPassDesc Pass;
                Pass.ColorAttachmentCount         = 1;
                Pass.ColorAttachments[0].Texture  = _BloomChain;
                Pass.ColorAttachments[0].MipLevel = DstLevel;
                Pass.ColorAttachments[0].Load     = RHI::LoadOp::Load;
                Pass.DebugName                    = "Bloom upsample";
                Commands.BeginRenderPass(Pass);
                Commands.BindPipeline(_UpsamplePipeline);

                UpsampleParams Params {};
                Params.TexelSize[0] = 1.0f / CAST<f32>(SourceMipWidth);
                Params.TexelSize[1] = 1.0f / CAST<f32>(SourceMipHeight);
                Params.Params2[0]   = CAST<f32>(Level);
                Commands.BindUniformBuffer(0, _Device->AllocateUniform(Params));
                Commands.BindTexture(0, _BloomChain, _Sampler);
                Commands.Draw(3);
                Commands.EndRenderPass();
            }

            Commands.PopDebugGroup();
        }

        Commands.PushDebugGroup("Post-process composite");

        RHI::RenderPassDesc Pass;
        Pass.ColorAttachmentCount        = 1;
        Pass.ColorAttachments[0].Texture = Target;
        // Preserves whatever was already drawn into Target this frame (2D
        // sprites) outside the alpha-blended area the scene actually wrote -
        // see the class comment.
        Pass.ColorAttachments[0].Load = RHI::LoadOp::Load;
        Pass.DebugName                = "Post-process composite";
        Commands.BeginRenderPass(Pass);
        Commands.BindPipeline(_CompositePipeline);

        CompositeParams Params {};
        Params.Params[0] = Settings_.Exposure;
        Params.Params[1] = UseBloom ? Settings_.BloomIntensity : 0.0f;
        Commands.BindUniformBuffer(0, _Device->AllocateUniform(Params));
        Commands.BindTexture(0, SceneColor, _Sampler);
        // Bound even with bloom off/unavailable (intensity 0 makes it
        // inert) - every declared slot is always bound, same convention as
        // MeshRenderer's material channels.
        Commands.BindTexture(1, UseBloom ? _BloomChain : SceneColor, _Sampler);
        Commands.Draw(3);
        Commands.EndRenderPass();

        Commands.PopDebugGroup();
    }
}  // namespace Xen
