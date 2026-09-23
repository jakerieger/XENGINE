//
// Created by Jake Rieger on 9/23/2026.
//

#include "SSAO.hpp"

#include <XenPAK/AssetRegistry.hpp>
#include <XenPAK/AssetBuffer.hpp>

namespace Xen {
    namespace {
        constexpr RHI::Format AoFormat = RHI::Format::R8_UNORM;

        // Matches SSAO.hlsl's cbuffer exactly.
        struct SsaoParams {
            Float4x4 ViewProjection;
            Float4x4 InvViewProjection;
            f32 CameraPositionAndPad[4];
            f32 Params2[4];  // x = radius, y = power, z = bias, w unused
        };

        // Matches SSAOBlur.hlsl's cbuffer exactly.
        struct BlurParams {
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

    SSAO::~SSAO() {
        Shutdown();
    }

    bool SSAO::Initialize(RHI::IRenderDevice& Device, const PAK::AssetRegistry& Assets) {
        _Device = &Device;

        const RHI::ShaderHandle Vertex =
          LoadShader(Device, Assets, ASSET("xen.shader.ssao.vs"), RHI::ShaderStage::Vertex, "XEN.Shaders.SSAO.vs");
        const RHI::ShaderHandle Fragment =
          LoadShader(Device, Assets, ASSET("xen.shader.ssao.ps"), RHI::ShaderStage::Fragment, "XEN.Shaders.SSAO.ps");

        RHI::PipelineLayoutDesc LayoutDesc;
        LayoutDesc.Binding(0, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment);
        LayoutDesc.DebugName = "XEN.Shaders.SSAO";
        _Layout              = Device.CreatePipelineLayout(LayoutDesc);

        if (Vertex.IsValid() && Fragment.IsValid() && _Layout.IsValid()) {
            RHI::GraphicsPipelineDesc Desc;
            Desc.VertexShader         = Vertex;
            Desc.FragmentShader       = Fragment;
            Desc.PipelineLayout       = _Layout;
            Desc.Topology             = RHI::PrimitiveTopology::TriangleList;
            Desc.ColorAttachmentCount = 1;
            Desc.ColorFormats[0]      = AoFormat;
            Desc.DebugName            = "XEN.Shaders.SSAO";
            _Pipeline                 = Device.CreateGraphicsPipeline(Desc);
        }

        if (Vertex.IsValid()) Device.DestroyShader(Vertex);
        if (Fragment.IsValid()) Device.DestroyShader(Fragment);

        const RHI::ShaderHandle BlurVertex = LoadShader(
          Device, Assets, ASSET("xen.shader.ssaoblur.vs"), RHI::ShaderStage::Vertex, "XEN.Shaders.SSAOBlur.vs");
        const RHI::ShaderHandle BlurFragment = LoadShader(
          Device, Assets, ASSET("xen.shader.ssaoblur.ps"), RHI::ShaderStage::Fragment, "XEN.Shaders.SSAOBlur.ps");

        RHI::PipelineLayoutDesc BlurLayoutDesc;
        BlurLayoutDesc.Binding(0, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment);
        BlurLayoutDesc.DebugName = "XEN.Shaders.SSAOBlur";
        _BlurLayout              = Device.CreatePipelineLayout(BlurLayoutDesc);

        if (BlurVertex.IsValid() && BlurFragment.IsValid() && _BlurLayout.IsValid()) {
            RHI::GraphicsPipelineDesc Desc;
            Desc.VertexShader         = BlurVertex;
            Desc.FragmentShader       = BlurFragment;
            Desc.PipelineLayout       = _BlurLayout;
            Desc.Topology             = RHI::PrimitiveTopology::TriangleList;
            Desc.ColorAttachmentCount = 1;
            Desc.ColorFormats[0]      = AoFormat;
            Desc.DebugName            = "XEN.Shaders.SSAOBlur";
            _BlurPipeline             = Device.CreateGraphicsPipeline(Desc);
        }

        if (BlurVertex.IsValid()) Device.DestroyShader(BlurVertex);
        if (BlurFragment.IsValid()) Device.DestroyShader(BlurFragment);

        if (!_Pipeline.IsValid() || !_BlurPipeline.IsValid()) {
            LOG_WARN("SSAO shaders not found or failed to compile - ambient occlusion will be unavailable");
        }

        RHI::SamplerDesc SamplerDesc;
        SamplerDesc.MinFilter  = RHI::FilterMode::Nearest;
        SamplerDesc.MagFilter  = RHI::FilterMode::Nearest;
        SamplerDesc.MipFilter  = RHI::MipMode::None;
        SamplerDesc.AddressU   = RHI::AddressMode::ClampToEdge;
        SamplerDesc.AddressV   = RHI::AddressMode::ClampToEdge;
        SamplerDesc.DebugName  = "XEN.SSAO";
        _Sampler               = Device.CreateSampler(SamplerDesc);

        if (!_Pipeline.IsValid() || !_BlurPipeline.IsValid() || !_Sampler.IsValid()) {
            Shutdown();
            return false;
        }
        return true;
    }

    void SSAO::Shutdown() {
        if (!_Device) return;

        if (_Pipeline.IsValid()) _Device->DestroyPipeline(_Pipeline);
        if (_Layout.IsValid()) _Device->DestroyPipelineLayout(_Layout);
        if (_BlurPipeline.IsValid()) _Device->DestroyPipeline(_BlurPipeline);
        if (_BlurLayout.IsValid()) _Device->DestroyPipelineLayout(_BlurLayout);
        if (_Sampler.IsValid()) _Device->DestroySampler(_Sampler);
        if (_RawTarget.IsValid()) _Device->DestroyTexture(_RawTarget);
        if (_BlurredTarget.IsValid()) _Device->DestroyTexture(_BlurredTarget);

        _Pipeline     = {};
        _Layout       = {};
        _BlurPipeline = {};
        _BlurLayout   = {};
        _Sampler      = {};
        _RawTarget      = {};
        _BlurredTarget  = {};
        _Width = _Height = 0;
        _Device           = nullptr;
    }

    void SSAO::EnsureTargets(const u32 Width, const u32 Height) {
        if (_RawTarget.IsValid() && _Width == Width && _Height == Height) return;

        if (_RawTarget.IsValid()) _Device->DestroyTexture(_RawTarget);
        if (_BlurredTarget.IsValid()) _Device->DestroyTexture(_BlurredTarget);

        RHI::TextureDesc Desc;
        Desc.Width     = Width;
        Desc.Height    = Height;
        Desc.Fmt       = AoFormat;
        Desc.Usage     = RHI::TextureUsage::ColorTarget | RHI::TextureUsage::Sampled;
        Desc.DebugName = "XEN.SSAO.Raw";
        _RawTarget     = _Device->CreateTexture(Desc);

        Desc.DebugName  = "XEN.SSAO.Blurred";
        _BlurredTarget  = _Device->CreateTexture(Desc);

        _Width  = Width;
        _Height = Height;
    }

    RHI::TextureHandle SSAO::Render(RHI::CommandBuffer& Commands,
                                    const RHI::TextureHandle Depth,
                                    const Float4x4& ViewProjection,
                                    const Float4x4& InvViewProjection,
                                    const Float3& CameraPosition,
                                    const u32 Width,
                                    const u32 Height,
                                    const Settings& Settings_) {
        if (!_Device || !_Pipeline.IsValid() || !_BlurPipeline.IsValid() || !Settings_.Enabled) return {};

        EnsureTargets(Width, Height);
        if (!_RawTarget.IsValid() || !_BlurredTarget.IsValid()) return {};

        Commands.PushDebugGroup("SSAO");

        {
            const RHI::RenderPassDesc Pass = RHI::RenderPassDesc::ColorTarget(_RawTarget, 1.0f, 1.0f, 1.0f, 1.0f);
            Commands.BeginRenderPass(Pass);
            Commands.BindPipeline(_Pipeline);

            SsaoParams Params {};
            Params.ViewProjection    = ViewProjection;
            Params.InvViewProjection = InvViewProjection;
            Params.CameraPositionAndPad[0] = CameraPosition.x;
            Params.CameraPositionAndPad[1] = CameraPosition.y;
            Params.CameraPositionAndPad[2] = CameraPosition.z;
            Params.Params2[0]              = Settings_.Radius;
            Params.Params2[1]              = Settings_.Power;
            Params.Params2[2]              = Settings_.Bias;
            Commands.BindUniformBuffer(0, _Device->AllocateUniform(Params));
            Commands.BindTexture(0, Depth, _Sampler);
            Commands.Draw(3);
            Commands.EndRenderPass();
        }

        {
            const RHI::RenderPassDesc Pass = RHI::RenderPassDesc::ColorTarget(_BlurredTarget);
            Commands.BeginRenderPass(Pass);
            Commands.BindPipeline(_BlurPipeline);

            BlurParams Params {};
            Params.TexelSize[0] = 1.0f / CAST<f32>(Width);
            Params.TexelSize[1] = 1.0f / CAST<f32>(Height);
            Commands.BindUniformBuffer(0, _Device->AllocateUniform(Params));
            Commands.BindTexture(0, _RawTarget, _Sampler);
            Commands.Draw(3);
            Commands.EndRenderPass();
        }

        Commands.PopDebugGroup();
        return _BlurredTarget;
    }
}  // namespace Xen
