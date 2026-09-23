//
// Created by Jake Rieger on 9/23/2026.
//

#include "FXAA.hpp"

#include <XenPAK/AssetRegistry.hpp>
#include <XenPAK/AssetBuffer.hpp>

#include <algorithm>

namespace Xen {
    namespace {
        // Matches FXAA.hlsl's cbuffer exactly.
        struct FxaaParams {
            f32 TexelSize[4];
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

    FXAA::~FXAA() {
        Shutdown();
    }

    bool FXAA::Initialize(RHI::IRenderDevice& Device, const PAK::AssetRegistry& Assets, const RHI::Format TargetFormat) {
        _Device       = &Device;
        _TargetFormat = TargetFormat;

        const RHI::ShaderHandle Vertex =
          LoadShader(Device, Assets, ASSET("xen.shader.fxaa.vs"), RHI::ShaderStage::Vertex, "XEN.Shaders.FXAA.vs");
        const RHI::ShaderHandle Fragment =
          LoadShader(Device, Assets, ASSET("xen.shader.fxaa.ps"), RHI::ShaderStage::Fragment, "XEN.Shaders.FXAA.ps");

        RHI::PipelineLayoutDesc LayoutDesc;
        LayoutDesc.Binding(0, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment);
        LayoutDesc.DebugName = "XEN.Shaders.FXAA";
        _Layout              = Device.CreatePipelineLayout(LayoutDesc);

        if (Vertex.IsValid() && Fragment.IsValid() && _Layout.IsValid()) {
            RHI::GraphicsPipelineDesc Desc;
            Desc.VertexShader         = Vertex;
            Desc.FragmentShader       = Fragment;
            Desc.PipelineLayout       = _Layout;
            Desc.Topology             = RHI::PrimitiveTopology::TriangleList;
            Desc.ColorAttachmentCount = 1;
            Desc.ColorFormats[0]      = TargetFormat;
            Desc.DebugName            = "XEN.Shaders.FXAA";
            _Pipeline                 = Device.CreateGraphicsPipeline(Desc);
        }

        if (Vertex.IsValid()) Device.DestroyShader(Vertex);
        if (Fragment.IsValid()) Device.DestroyShader(Fragment);

        if (!_Pipeline.IsValid()) {
            LOG_WARN("FXAA shader not found or failed to compile - anti-aliasing will be unavailable");
        }

        RHI::SamplerDesc SamplerDesc;
        SamplerDesc.MipFilter   = RHI::MipMode::None;  // the source has exactly one mip; every sample picks it implicitly
        SamplerDesc.AddressU    = RHI::AddressMode::ClampToEdge;
        SamplerDesc.AddressV    = RHI::AddressMode::ClampToEdge;
        SamplerDesc.DebugName   = "XEN.FXAA";
        _Sampler                = Device.CreateSampler(SamplerDesc);

        if (!_Pipeline.IsValid() || !_Sampler.IsValid()) {
            Shutdown();
            return false;
        }
        return true;
    }

    void FXAA::Shutdown() {
        if (!_Device) return;

        if (_Pipeline.IsValid()) _Device->DestroyPipeline(_Pipeline);
        if (_Layout.IsValid()) _Device->DestroyPipelineLayout(_Layout);
        if (_Sampler.IsValid()) _Device->DestroySampler(_Sampler);
        if (_Scratch.IsValid()) _Device->DestroyTexture(_Scratch);

        _Pipeline = {};
        _Layout   = {};
        _Sampler  = {};
        _Scratch  = {};
        _ScratchWidth = _ScratchHeight = 0;
        _Device                        = nullptr;
    }

    void FXAA::EnsureScratch(const u32 Width, const u32 Height) {
        if (_Scratch.IsValid() && _ScratchWidth == Width && _ScratchHeight == Height) return;

        if (_Scratch.IsValid()) _Device->DestroyTexture(_Scratch);

        RHI::TextureDesc Desc;
        Desc.Width     = Width;
        Desc.Height    = Height;
        Desc.Fmt       = _TargetFormat;
        Desc.Usage     = RHI::TextureUsage::ColorTarget | RHI::TextureUsage::Sampled;
        Desc.DebugName = "XEN.FXAA.Scratch";

        _Scratch      = _Device->CreateTexture(Desc);
        _ScratchWidth  = Width;
        _ScratchHeight = Height;
    }

    RHI::TextureHandle FXAA::Render(const RHI::TextureHandle Source,
                                    const u32 Width,
                                    const u32 Height,
                                    const Settings& Settings_) {
        if (!_Device || !_Pipeline.IsValid() || !Settings_.Enabled) return Source;

        EnsureScratch(Width, Height);
        if (!_Scratch.IsValid()) return Source;

        _Commands.Reset();
        _Commands.PushDebugGroup("FXAA");

        const RHI::RenderPassDesc Pass = RHI::RenderPassDesc::ColorTarget(_Scratch);
        _Commands.BeginRenderPass(Pass);
        _Commands.BindPipeline(_Pipeline);

        FxaaParams Params {};
        Params.TexelSize[0] = 1.0f / CAST<f32>(Width);
        Params.TexelSize[1] = 1.0f / CAST<f32>(Height);
        _Commands.BindUniformBuffer(0, _Device->AllocateUniform(Params));
        _Commands.BindTexture(0, Source, _Sampler);
        _Commands.Draw(3);
        _Commands.EndRenderPass();

        _Commands.PopDebugGroup();
        _Device->Submit(_Commands);

        return _Scratch;
    }
}  // namespace Xen
