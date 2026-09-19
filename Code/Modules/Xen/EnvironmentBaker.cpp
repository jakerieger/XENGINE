//
// Created by Jake Rieger on 9/19/2026.
//

#include "EnvironmentBaker.hpp"

#include <XenPAK/AssetRegistry.hpp>
#include <XenPAK/AssetBuffer.hpp>

#include <algorithm>

namespace Xen {
    namespace {
        // The prefiltered map's base (roughness 0) width is capped here: a
        // mirror-sharp reflection of a 1024x512 map is plenty, and the bake
        // cost and memory both grow with the base size.
        constexpr u32 MaxBaseWidth = 1024;

        // Mips run down to a 16-texel-wide level (roughness 1) or this many,
        // whichever comes first - below that the lobe is so wide every texel
        // is nearly the same color anyway.
        constexpr u32 MaxLevels    = 7;
        constexpr u32 SmallestWidth = 16;

        constexpr u32 IrradianceWidth  = 64;
        constexpr u32 IrradianceHeight = 32;

        constexpr f32 PrefilterSampleCount  = 512.0f;
        constexpr f32 IrradianceSampleCount = 2048.0f;

        constexpr RHI::Format BakeFormat = RHI::Format::RGBA16_FLOAT;

        // Matches the cbuffer both bake shaders declare at b0.
        struct BakeParams {
            f32 Roughness;
            f32 SampleCount;
            f32 DestWidth;
            f32 Pad;
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

    EnvironmentBaker::~EnvironmentBaker() {
        Shutdown();
    }

    bool EnvironmentBaker::Initialize(RHI::IRenderDevice& Device, const PAK::AssetRegistry& Assets) {
        const RHI::ShaderHandle PrefilterVs = LoadShader(Device,
                                                         Assets,
                                                         ASSET("xen.shader.prefilterenvironment.vs"),
                                                         RHI::ShaderStage::Vertex,
                                                         "XEN.Shaders.PrefilterEnvironment.vs");
        const RHI::ShaderHandle PrefilterPs = LoadShader(Device,
                                                         Assets,
                                                         ASSET("xen.shader.prefilterenvironment.ps"),
                                                         RHI::ShaderStage::Fragment,
                                                         "XEN.Shaders.PrefilterEnvironment.ps");
        const RHI::ShaderHandle IrradianceVs = LoadShader(Device,
                                                          Assets,
                                                          ASSET("xen.shader.irradianceconvolve.vs"),
                                                          RHI::ShaderStage::Vertex,
                                                          "XEN.Shaders.IrradianceConvolve.vs");
        const RHI::ShaderHandle IrradiancePs = LoadShader(Device,
                                                          Assets,
                                                          ASSET("xen.shader.irradianceconvolve.ps"),
                                                          RHI::ShaderStage::Fragment,
                                                          "XEN.Shaders.IrradianceConvolve.ps");

        // Both passes bind the same thing: b0 = bake parameters, t0/s0 = the
        // source environment map.
        RHI::PipelineLayoutDesc LayoutDesc;
        LayoutDesc.Binding(0, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment);
        LayoutDesc.DebugName = "XEN.Shaders.EnvironmentBake";
        _Layout              = Device.CreatePipelineLayout(LayoutDesc);

        const auto MakePipeline = [&](const RHI::ShaderHandle Vertex, const RHI::ShaderHandle Fragment, const char* Name) {
            if (!Vertex.IsValid() || !Fragment.IsValid() || !_Layout.IsValid()) return RHI::PipelineHandle {};

            RHI::GraphicsPipelineDesc Desc;
            Desc.VertexShader         = Vertex;
            Desc.FragmentShader       = Fragment;
            Desc.PipelineLayout       = _Layout;
            Desc.Topology             = RHI::PrimitiveTopology::TriangleList;
            Desc.ColorAttachmentCount = 1;
            Desc.ColorFormats[0]      = BakeFormat;
            Desc.DebugName            = Name;
            return Device.CreateGraphicsPipeline(Desc);
        };

        _PrefilterPipeline  = MakePipeline(PrefilterVs, PrefilterPs, "XEN.Shaders.PrefilterEnvironment");
        _IrradiancePipeline = MakePipeline(IrradianceVs, IrradiancePs, "XEN.Shaders.IrradianceConvolve");

        for (const RHI::ShaderHandle Shader : {PrefilterVs, PrefilterPs, IrradianceVs, IrradiancePs}) {
            if (Shader.IsValid()) Device.DestroyShader(Shader);
        }

        // Longitude wraps (U repeats), latitude doesn't (V clamps at the
        // poles); linear across mips so a sample's LOD picks a blend.
        RHI::SamplerDesc SamplerDesc;
        SamplerDesc.AddressU  = RHI::AddressMode::Repeat;
        SamplerDesc.AddressV  = RHI::AddressMode::ClampToEdge;
        SamplerDesc.DebugName = "XEN.EnvironmentBake";
        _Sampler              = Device.CreateSampler(SamplerDesc);

        _Device = &Device;
        if (!_PrefilterPipeline.IsValid() || !_IrradiancePipeline.IsValid() || !_Sampler.IsValid()) {
            Shutdown();
            return false;
        }

        return true;
    }

    void EnvironmentBaker::Shutdown() {
        if (!_Device) return;

        if (_PrefilterPipeline.IsValid()) _Device->DestroyPipeline(_PrefilterPipeline);
        if (_IrradiancePipeline.IsValid()) _Device->DestroyPipeline(_IrradiancePipeline);
        if (_Layout.IsValid()) _Device->DestroyPipelineLayout(_Layout);
        if (_Sampler.IsValid()) _Device->DestroySampler(_Sampler);

        _PrefilterPipeline  = {};
        _IrradiancePipeline = {};
        _Layout             = {};
        _Sampler            = {};
        _Device             = nullptr;
    }

    bool EnvironmentBaker::Bake(const RHI::TextureHandle Source, const u32 SourceWidth, Result& Out) {
        if (!_Device || !Source.IsValid() || SourceWidth == 0) return false;

        const u32 BaseWidth  = std::min(SourceWidth, MaxBaseWidth);
        const u32 BaseHeight = std::max(BaseWidth / 2, 1u);

        // floor(log2(BaseWidth)) - log2(SmallestWidth) + 1 levels, clamped.
        u32 Levels = 1;
        for (u32 Width = BaseWidth; Width > SmallestWidth && Levels < MaxLevels; Width /= 2) ++Levels;

        RHI::TextureDesc PrefilteredDesc;
        PrefilteredDesc.Width     = BaseWidth;
        PrefilteredDesc.Height    = BaseHeight;
        PrefilteredDesc.MipLevels = Levels;
        PrefilteredDesc.Fmt       = BakeFormat;
        PrefilteredDesc.Usage     = RHI::TextureUsage::ColorTarget | RHI::TextureUsage::Sampled;
        PrefilteredDesc.DebugName = "XEN.Environment.Prefiltered";

        RHI::TextureDesc IrradianceDesc;
        IrradianceDesc.Width     = IrradianceWidth;
        IrradianceDesc.Height    = IrradianceHeight;
        IrradianceDesc.Fmt       = BakeFormat;
        IrradianceDesc.Usage     = RHI::TextureUsage::ColorTarget | RHI::TextureUsage::Sampled;
        IrradianceDesc.DebugName = "XEN.Environment.Irradiance";

        const RHI::TextureHandle Prefiltered = _Device->CreateTexture(PrefilteredDesc);
        const RHI::TextureHandle Irradiance  = _Device->CreateTexture(IrradianceDesc);
        if (!Prefiltered.IsValid() || !Irradiance.IsValid()) {
            if (Prefiltered.IsValid()) _Device->DestroyTexture(Prefiltered);
            if (Irradiance.IsValid()) _Device->DestroyTexture(Irradiance);
            return false;
        }

        _Commands.Reset();
        _Commands.PushDebugGroup("Environment bake");

        const auto RecordPass = [&](const RHI::TextureHandle Target,
                                    const u32 Mip,
                                    const RHI::PipelineHandle Pipeline,
                                    const BakeParams& Params) {
            RHI::RenderPassDesc Pass = RHI::RenderPassDesc::ColorTarget(Target, 0.0f, 0.0f, 0.0f, 1.0f);
            Pass.ColorAttachments[0].MipLevel = Mip;
            Pass.DebugName                    = "Environment bake";

            _Commands.BeginRenderPass(Pass);
            _Commands.BindPipeline(Pipeline);
            _Commands.BindUniformBuffer(0, _Device->AllocateUniform(Params));
            _Commands.BindTexture(0, Source, _Sampler);
            _Commands.Draw(3);
            _Commands.EndRenderPass();
        };

        // Mip m is roughness m / (Levels - 1): PBR.hlsl maps roughness back to
        // a mip the same way (Roughness * (levels - 1)), so the two must agree.
        for (u32 Mip = 0; Mip < Levels; ++Mip) {
            const f32 Roughness = Levels > 1 ? CAST<f32>(Mip) / CAST<f32>(Levels - 1) : 0.0f;
            const f32 DestWidth = CAST<f32>(std::max(BaseWidth >> Mip, 1u));
            RecordPass(Prefiltered, Mip, _PrefilterPipeline, BakeParams {Roughness, PrefilterSampleCount, DestWidth, 0.0f});
        }

        RecordPass(Irradiance,
                   0,
                   _IrradiancePipeline,
                   BakeParams {0.0f, IrradianceSampleCount, CAST<f32>(IrradianceWidth), 0.0f});

        _Commands.PopDebugGroup();
        _Device->Submit(_Commands);

        Out.Prefiltered = Prefiltered;
        Out.Irradiance  = Irradiance;
        return true;
    }
}  // namespace Xen
