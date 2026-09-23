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
        // Single-channel is enough for a luminance value - keeps the
        // metering chain's textures (one per level, see EnsureLuminanceChain)
        // cheap.
        constexpr RHI::Format LuminanceFormat = RHI::Format::R16_FLOAT;

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
            f32 Params2[4];
        };

        // Matches LuminanceMeasure.hlsl / LuminanceReduce.hlsl's cbuffers -
        // identical shape, shared by both.
        struct LuminanceTexelSizeParams {
            f32 TexelSize[4];
        };

        // Matches LuminanceAdapt.hlsl's cbuffer.
        struct LuminanceAdaptParams {
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
          .Binding(1, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment)
          .Binding(2, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Fragment)
          .Binding(2, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment);
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

        // Auto exposure's luminance-metering chain (see PostProcess.hpp):
        // Measure and Reduce share one layout - both are just "b0 params, t0/
        // s0 the one texture they read", the same shape as _BloomLayout.
        const RHI::ShaderHandle LumMeasureVs = LoadShader(Device,
                                                          Assets,
                                                          ASSET("xen.shader.luminancemeasure.vs"),
                                                          RHI::ShaderStage::Vertex,
                                                          "XEN.Shaders.LuminanceMeasure.vs");
        const RHI::ShaderHandle LumMeasurePs = LoadShader(Device,
                                                          Assets,
                                                          ASSET("xen.shader.luminancemeasure.ps"),
                                                          RHI::ShaderStage::Fragment,
                                                          "XEN.Shaders.LuminanceMeasure.ps");
        const RHI::ShaderHandle LumReduceVs = LoadShader(Device,
                                                         Assets,
                                                         ASSET("xen.shader.luminancereduce.vs"),
                                                         RHI::ShaderStage::Vertex,
                                                         "XEN.Shaders.LuminanceReduce.vs");
        const RHI::ShaderHandle LumReducePs = LoadShader(Device,
                                                         Assets,
                                                         ASSET("xen.shader.luminancereduce.ps"),
                                                         RHI::ShaderStage::Fragment,
                                                         "XEN.Shaders.LuminanceReduce.ps");
        const RHI::ShaderHandle LumAdaptVs = LoadShader(
          Device, Assets, ASSET("xen.shader.luminanceadapt.vs"), RHI::ShaderStage::Vertex, "XEN.Shaders.LuminanceAdapt.vs");
        const RHI::ShaderHandle LumAdaptPs = LoadShader(Device,
                                                        Assets,
                                                        ASSET("xen.shader.luminanceadapt.ps"),
                                                        RHI::ShaderStage::Fragment,
                                                        "XEN.Shaders.LuminanceAdapt.ps");

        RHI::PipelineLayoutDesc LumLayoutDesc;
        LumLayoutDesc.Binding(0, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment);
        LumLayoutDesc.DebugName = "XEN.Shaders.LuminanceMeter";
        _LumLayout              = Device.CreatePipelineLayout(LumLayoutDesc);

        const auto MakeLumPipeline =
          [&](const RHI::ShaderHandle Vertex, const RHI::ShaderHandle Fragment, const char* Name) {
              if (!Vertex.IsValid() || !Fragment.IsValid() || !_LumLayout.IsValid()) return RHI::PipelineHandle {};

              RHI::GraphicsPipelineDesc Desc;
              Desc.VertexShader         = Vertex;
              Desc.FragmentShader       = Fragment;
              Desc.PipelineLayout       = _LumLayout;
              Desc.Topology             = RHI::PrimitiveTopology::TriangleList;
              Desc.ColorAttachmentCount = 1;
              Desc.ColorFormats[0]      = LuminanceFormat;
              Desc.DebugName            = Name;
              return Device.CreateGraphicsPipeline(Desc);
          };

        _LumMeasurePipeline = MakeLumPipeline(LumMeasureVs, LumMeasurePs, "XEN.Shaders.LuminanceMeasure");
        _LumReducePipeline  = MakeLumPipeline(LumReduceVs, LumReducePs, "XEN.Shaders.LuminanceReduce");

        RHI::PipelineLayoutDesc LumAdaptLayoutDesc;
        LumAdaptLayoutDesc.Binding(0, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment)
          .Binding(1, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Fragment)
          .Binding(1, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment);
        LumAdaptLayoutDesc.DebugName = "XEN.Shaders.LuminanceAdapt";
        _LumAdaptLayout              = Device.CreatePipelineLayout(LumAdaptLayoutDesc);

        if (LumAdaptVs.IsValid() && LumAdaptPs.IsValid() && _LumAdaptLayout.IsValid()) {
            RHI::GraphicsPipelineDesc Desc;
            Desc.VertexShader         = LumAdaptVs;
            Desc.FragmentShader       = LumAdaptPs;
            Desc.PipelineLayout       = _LumAdaptLayout;
            Desc.Topology             = RHI::PrimitiveTopology::TriangleList;
            Desc.ColorAttachmentCount = 1;
            Desc.ColorFormats[0]      = LuminanceFormat;
            Desc.DebugName            = "XEN.Shaders.LuminanceAdapt";
            _LumAdaptPipeline         = Device.CreateGraphicsPipeline(Desc);
        }

        for (const RHI::ShaderHandle Shader : {LumMeasureVs, LumMeasurePs, LumReduceVs, LumReducePs, LumAdaptVs, LumAdaptPs}) {
            if (Shader.IsValid()) Device.DestroyShader(Shader);
        }

        if (!_LumMeasurePipeline.IsValid() || !_LumReducePipeline.IsValid() || !_LumAdaptPipeline.IsValid()) {
            LOG_WARN("auto exposure shaders not found or failed to compile - auto exposure will be "
                     "unavailable (falls back to Settings::Exposure)");
        }

        RHI::SamplerDesc SamplerDesc;
        SamplerDesc.MipFilter   = RHI::MipMode::None;  // every sample picks its mip explicitly (SampleLevel)
        SamplerDesc.AddressU    = RHI::AddressMode::ClampToEdge;
        SamplerDesc.AddressV    = RHI::AddressMode::ClampToEdge;
        SamplerDesc.DebugName   = "XEN.PostProcess";
        _Sampler                = Device.CreateSampler(SamplerDesc);

        // The two ping-ponged adapted-luminance textures never resize (see
        // the class comment), so they're created once here rather than in
        // EnsureLuminanceChain - only if the metering pipelines actually
        // built, so a PostProcess whose auto-exposure shaders are missing
        // doesn't carry two pointless 1x1 textures for its whole lifetime.
        if (_LumMeasurePipeline.IsValid() && _LumReducePipeline.IsValid() && _LumAdaptPipeline.IsValid()) {
            RHI::TextureDesc AdaptDesc;
            AdaptDesc.Width       = 1;
            AdaptDesc.Height      = 1;
            AdaptDesc.Fmt         = LuminanceFormat;
            AdaptDesc.Usage       = RHI::TextureUsage::ColorTarget | RHI::TextureUsage::Sampled;
            AdaptDesc.DebugName   = "XEN.PostProcess.AdaptedLuminance";
            _AdaptedLuminance[0]  = Device.CreateTexture(AdaptDesc);
            _AdaptedLuminance[1]  = Device.CreateTexture(AdaptDesc);
        }

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
        for (const RHI::TextureHandle Scratch : _Scratches) {
            if (Scratch.IsValid()) _Device->DestroyTexture(Scratch);
        }

        if (_LumMeasurePipeline.IsValid()) _Device->DestroyPipeline(_LumMeasurePipeline);
        if (_LumReducePipeline.IsValid()) _Device->DestroyPipeline(_LumReducePipeline);
        if (_LumLayout.IsValid()) _Device->DestroyPipelineLayout(_LumLayout);
        if (_LumAdaptPipeline.IsValid()) _Device->DestroyPipeline(_LumAdaptPipeline);
        if (_LumAdaptLayout.IsValid()) _Device->DestroyPipelineLayout(_LumAdaptLayout);
        for (const RHI::TextureHandle Level : _LumChain) {
            if (Level.IsValid()) _Device->DestroyTexture(Level);
        }
        for (const RHI::TextureHandle Adapted : _AdaptedLuminance) {
            if (Adapted.IsValid()) _Device->DestroyTexture(Adapted);
        }

        _DownsamplePipeline = {};
        _UpsamplePipeline   = {};
        _BloomLayout        = {};
        _CompositePipeline  = {};
        _CompositeLayout    = {};
        _Sampler            = {};
        _BloomChain         = {};
        _BloomWidth = _BloomHeight = _BloomLevels = 0;
        _Scratches.clear();

        _LumMeasurePipeline = {};
        _LumReducePipeline  = {};
        _LumLayout          = {};
        _LumAdaptPipeline   = {};
        _LumAdaptLayout     = {};
        _LumChain.clear();
        _LumBaseWidth = _LumBaseHeight = 0;
        _AdaptedLuminance[0] = _AdaptedLuminance[1] = {};
        _AdaptedLuminanceIndex  = 0;
        _AdaptedLuminancePrimed = false;

        _Device = nullptr;
    }

    void PostProcess::EnsureBloomChain(const u32 SceneWidth, const u32 SceneHeight) {
        const u32 Width  = std::max(SceneWidth / 2, 1u);
        const u32 Height = std::max(SceneHeight / 2, 1u);
        if (_BloomChain.IsValid() && _BloomWidth == Width && _BloomHeight == Height) return;

        if (_BloomChain.IsValid()) _Device->DestroyTexture(_BloomChain);
        for (const RHI::TextureHandle Scratch : _Scratches) {
            if (Scratch.IsValid()) _Device->DestroyTexture(Scratch);
        }
        _Scratches.clear();

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

        // One persistent scratch per level, sized to match that level of
        // _BloomChain exactly - see the class comment (PostProcess.hpp) for
        // why one reused, resized-per-level scratch doesn't work here the
        // way it does for MipGenerator's one-shot, wait-between-levels case.
        if (_BloomChain.IsValid()) {
            _Scratches.resize(_BloomLevels);
            for (u32 Level = 0; Level < _BloomLevels; ++Level) {
                RHI::TextureDesc ScratchDesc;
                ScratchDesc.Width     = std::max(Width >> Level, 1u);
                ScratchDesc.Height    = std::max(Height >> Level, 1u);
                ScratchDesc.Fmt       = BloomFormat;
                ScratchDesc.Usage = RHI::TextureUsage::ColorTarget | RHI::TextureUsage::Sampled | RHI::TextureUsage::CopyDst;
                ScratchDesc.DebugName = "XEN.PostProcess.Scratch";
                _Scratches[Level]     = _Device->CreateTexture(ScratchDesc);
            }
        }
    }

    void PostProcess::EnsureLuminanceChain(const u32 SceneWidth, const u32 SceneHeight) {
        const u32 Width  = std::max(SceneWidth / 2, 1u);
        const u32 Height = std::max(SceneHeight / 2, 1u);
        if (!_LumChain.empty() && _LumBaseWidth == Width && _LumBaseHeight == Height) return;

        for (const RHI::TextureHandle Level : _LumChain) {
            if (Level.IsValid()) _Device->DestroyTexture(Level);
        }
        _LumChain.clear();

        // Halve all the way down to exactly 1x1 - unlike _BloomChain, there's
        // no early stop at 8 texels: LuminanceAdapt.hlsl reads the last level
        // with one texel fetch and needs it to actually be the whole frame's
        // average, not a coarse-but-not-quite-there approximation of it.
        u32 Levels = 1;
        for (u32 W = Width, H = Height; W > 1 || H > 1; W = std::max(W / 2, 1u), H = std::max(H / 2, 1u)) ++Levels;

        _LumChain.resize(Levels);
        u32 LevelWidth = Width, LevelHeight = Height;
        for (u32 Level = 0; Level < Levels; ++Level) {
            RHI::TextureDesc Desc;
            Desc.Width     = LevelWidth;
            Desc.Height    = LevelHeight;
            Desc.Fmt       = LuminanceFormat;
            Desc.Usage     = RHI::TextureUsage::ColorTarget | RHI::TextureUsage::Sampled;
            Desc.DebugName = "XEN.PostProcess.Luminance";
            _LumChain[Level] = _Device->CreateTexture(Desc);

            LevelWidth  = std::max(LevelWidth / 2, 1u);
            LevelHeight = std::max(LevelHeight / 2, 1u);
        }

        _LumBaseWidth  = Width;
        _LumBaseHeight = Height;
    }

    void PostProcess::Render(RHI::CommandBuffer& Commands,
                             const RHI::TextureHandle SceneColor,
                             const u32 SceneWidth,
                             const u32 SceneHeight,
                             const RHI::TextureHandle Target,
                             const f32 DeltaTime,
                             const Settings& Settings_) {
        if (!_Device || !_CompositePipeline.IsValid()) return;

        const bool WantsBloom = Settings_.BloomEnabled && _DownsamplePipeline.IsValid() && _UpsamplePipeline.IsValid();
        if (WantsBloom) EnsureBloomChain(SceneWidth, SceneHeight);
        const bool UseBloom = WantsBloom && _BloomChain.IsValid();

        const bool WantsAutoExposure = Settings_.AutoExposureEnabled && _LumMeasurePipeline.IsValid() &&
          _LumReducePipeline.IsValid() && _LumAdaptPipeline.IsValid() && _AdaptedLuminance[0].IsValid() &&
          _AdaptedLuminance[1].IsValid();
        if (WantsAutoExposure) EnsureLuminanceChain(SceneWidth, SceneHeight);
        const bool UseAutoExposure = WantsAutoExposure && !_LumChain.empty();

        if (UseBloom) {
            Commands.PushDebugGroup("Bloom");

            // Downsample: level 0 reads the scene (thresholded), every level
            // after reads the chain's own previous level - copied out to a
            // scratch texture first, since _BloomChain is this same pass's
            // own render target (a different mip, but see
            // CommandBuffer::CopyTexture's comment for why that still
            // matters). Level 0 needs no copy: SceneColor is already a
            // separate texture from _BloomChain.
            u32 SourceWidth = SceneWidth, SourceHeight = SceneHeight;
            for (u32 Level = 0; Level < _BloomLevels; ++Level) {
                RHI::TextureHandle Source = SceneColor;
                if (Level > 0) {
                    Source = _Scratches[Level - 1];
                    Commands.CopyTexture(_BloomChain, Level - 1, Source, 0);
                }

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
                Params.Threshold[3] = 0.0f;  // Source is mip 0 either way (SceneColor, or the scratch copy)
                Commands.BindUniformBuffer(0, _Device->AllocateUniform(Params));
                Commands.BindTexture(0, Source, _Sampler);
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

                // Same reasoning as the downsample loop above: read the
                // smaller mip through a scratch copy, not _BloomChain
                // directly, since _BloomChain's own DstLevel is this pass's
                // render target.
                const RHI::TextureHandle Source = _Scratches[Level];
                Commands.CopyTexture(_BloomChain, Level, Source, 0);

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
                Params.Params2[0]   = 0.0f;  // the scratch texture has exactly one mip
                Commands.BindUniformBuffer(0, _Device->AllocateUniform(Params));
                Commands.BindTexture(0, Source, _Sampler);
                Commands.Draw(3);
                Commands.EndRenderPass();
            }

            Commands.PopDebugGroup();
        }

        if (UseAutoExposure) {
            Commands.PushDebugGroup("Auto exposure metering");

            // Measure: level 0 is a box-downsample + log2 conversion of the
            // scene directly - the only level that reads SceneColor.
            {
                RHI::RenderPassDesc Pass = RHI::RenderPassDesc::ColorTarget(_LumChain[0]);
                Pass.DebugName           = "Meter luminance";
                Commands.BeginRenderPass(Pass);
                Commands.BindPipeline(_LumMeasurePipeline);

                LuminanceTexelSizeParams Params {};
                Params.TexelSize[0] = 1.0f / CAST<f32>(SceneWidth);
                Params.TexelSize[1] = 1.0f / CAST<f32>(SceneHeight);
                Commands.BindUniformBuffer(0, _Device->AllocateUniform(Params));
                Commands.BindTexture(0, SceneColor, _Sampler);
                Commands.Draw(3);
                Commands.EndRenderPass();
            }

            // Reduce: halve down to exactly 1x1. Every level here is its own
            // texture (see the class comment), unlike _BloomChain's mips, so
            // level k reading level k-1 needs no CommandBuffer::CopyTexture
            // detour - there's no same-resource read/render-target conflict
            // to route around in the first place.
            u32 SourceWidth = _LumBaseWidth, SourceHeight = _LumBaseHeight;
            for (size_t Level = 1; Level < _LumChain.size(); ++Level) {
                RHI::RenderPassDesc Pass = RHI::RenderPassDesc::ColorTarget(_LumChain[Level]);
                Pass.DebugName           = "Reduce luminance";
                Commands.BeginRenderPass(Pass);
                Commands.BindPipeline(_LumReducePipeline);

                LuminanceTexelSizeParams Params {};
                Params.TexelSize[0] = 1.0f / CAST<f32>(SourceWidth);
                Params.TexelSize[1] = 1.0f / CAST<f32>(SourceHeight);
                Commands.BindUniformBuffer(0, _Device->AllocateUniform(Params));
                Commands.BindTexture(0, _LumChain[Level - 1], _Sampler);
                Commands.Draw(3);
                Commands.EndRenderPass();

                SourceWidth  = std::max(SourceWidth / 2, 1u);
                SourceHeight = std::max(SourceHeight / 2, 1u);
            }

            // Adapt: blend the previous frame's adapted luminance toward
            // this frame's measured value, writing the *other* ping-pong
            // slot - see the class comment for why this can't be done in
            // place (the composite pass below then reads whichever slot
            // this just wrote).
            const u32 PrevIndex = _AdaptedLuminanceIndex;
            const u32 NextIndex = PrevIndex ^ 1;

            RHI::RenderPassDesc Pass = RHI::RenderPassDesc::ColorTarget(_AdaptedLuminance[NextIndex]);
            Pass.DebugName           = "Adapt luminance";
            Commands.BeginRenderPass(Pass);
            Commands.BindPipeline(_LumAdaptPipeline);

            LuminanceAdaptParams AdaptParams {};
            AdaptParams.Params[0] = DeltaTime;
            AdaptParams.Params[1] =
              Settings_.AutoExposureAdaptUpSeconds > 1e-4f ? 1.0f / Settings_.AutoExposureAdaptUpSeconds : 1000.0f;
            AdaptParams.Params[2] =
              Settings_.AutoExposureAdaptDownSeconds > 1e-4f ? 1.0f / Settings_.AutoExposureAdaptDownSeconds : 1000.0f;
            AdaptParams.Params[3] = _AdaptedLuminancePrimed ? 0.0f : 1.0f;
            Commands.BindUniformBuffer(0, _Device->AllocateUniform(AdaptParams));
            Commands.BindTexture(0, _AdaptedLuminance[PrevIndex], _Sampler);
            Commands.BindTexture(1, _LumChain.back(), _Sampler);
            Commands.Draw(3);
            Commands.EndRenderPass();

            _AdaptedLuminanceIndex  = NextIndex;
            _AdaptedLuminancePrimed = true;

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
        Params.Params[0]  = Settings_.Exposure;
        Params.Params[1]  = UseBloom ? Settings_.BloomIntensity : 0.0f;
        Params.Params2[0] = UseAutoExposure ? 1.0f : 0.0f;
        Params.Params2[1] = Settings_.AutoExposureKey;
        Params.Params2[2] = Settings_.AutoExposureMinLuminance;
        Params.Params2[3] = Settings_.AutoExposureMaxLuminance;
        Commands.BindUniformBuffer(0, _Device->AllocateUniform(Params));
        Commands.BindTexture(0, SceneColor, _Sampler);
        // Bound even with bloom/auto exposure off or unavailable (inert in
        // both cases - intensity 0, or Params2.x reading 0) - every declared
        // slot is always bound, same convention as MeshRenderer's material
        // channels.
        Commands.BindTexture(1, UseBloom ? _BloomChain : SceneColor, _Sampler);
        Commands.BindTexture(2, UseAutoExposure ? _AdaptedLuminance[_AdaptedLuminanceIndex] : SceneColor, _Sampler);
        Commands.Draw(3);
        Commands.EndRenderPass();

        Commands.PopDebugGroup();
    }
}  // namespace Xen
