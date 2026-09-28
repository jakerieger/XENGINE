//
// Created by Jake Rieger on 9/23/2026.
//

#include "TAA.hpp"

#include <XenPAK/AssetRegistry.hpp>
#include <XenPAK/AssetBuffer.hpp>

namespace Xen {
    namespace {
        // Matches TAAResolve.hlsl's cbuffer exactly.
        struct TaaParams {
            f32 Params[4];  // x = BlendFactor, y = Primed (0/1), zw unused
        };

        // Base-N radical inverse - Halton(2,3) (RadicalInverse(Index, 2) and
        // RadicalInverse(Index, 3) together) is the standard low-discrepancy
        // 2D jitter sequence TAA implementations use (Unreal, Unity, id Tech
        // all use this exact pair). Common.hlsli's RadicalInverseVdC is the
        // same base-2 function already used GPU-side for IBL/SSAO sampling,
        // but jitter has to be known before the projection matrix is built,
        // so this is computed CPU-side instead - its own small copy rather
        // than a shared one.
        f32 RadicalInverse(u32 Index, const u32 Base) {
            f32 Result      = 0.0f;
            f32 Denominator = 1.0f;
            while (Index > 0) {
                Denominator *= CAST<f32>(Base);
                Result += CAST<f32>(Index % Base) / Denominator;
                Index /= Base;
            }
            return Result;
        }

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

    TAA::~TAA() {
        Shutdown();
    }

    bool TAA::Initialize(RHI::IRenderDevice& Device, const PAK::AssetRegistry& Assets, const RHI::Format ColorFormat) {
        _Device      = &Device;
        _ColorFormat = ColorFormat;

        const RHI::ShaderHandle Vertex = LoadShader(
          Device, Assets, ASSET("xen.shader.taaresolve.vs"), RHI::ShaderStage::Vertex, "XEN.Shaders.TAAResolve.vs");
        const RHI::ShaderHandle Fragment = LoadShader(
          Device, Assets, ASSET("xen.shader.taaresolve.ps"), RHI::ShaderStage::Fragment, "XEN.Shaders.TAAResolve.ps");

        RHI::PipelineLayoutDesc LayoutDesc;
        LayoutDesc.Binding(0, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment)
          .Binding(1, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Fragment)
          .Binding(1, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment)
          .Binding(2, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Fragment)
          .Binding(2, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment);
        LayoutDesc.DebugName = "XEN.Shaders.TAAResolve";
        _Layout              = Device.CreatePipelineLayout(LayoutDesc);

        if (Vertex.IsValid() && Fragment.IsValid() && _Layout.IsValid()) {
            RHI::GraphicsPipelineDesc Desc;
            Desc.VertexShader         = Vertex;
            Desc.FragmentShader       = Fragment;
            Desc.PipelineLayout       = _Layout;
            Desc.Topology             = RHI::PrimitiveTopology::TriangleList;
            Desc.ColorAttachmentCount = 1;
            Desc.ColorFormats[0]      = ColorFormat;
            Desc.DebugName            = "XEN.Shaders.TAAResolve";
            _Pipeline                 = Device.CreateGraphicsPipeline(Desc);
        }

        if (Vertex.IsValid()) Device.DestroyShader(Vertex);
        if (Fragment.IsValid()) Device.DestroyShader(Fragment);

        if (!_Pipeline.IsValid()) {
            LOG_WARN("TAA resolve shader not found or failed to compile - temporal anti-aliasing will be unavailable");
        }

        RHI::SamplerDesc SamplerDesc;
        SamplerDesc.MipFilter  = RHI::MipMode::None;
        SamplerDesc.AddressU   = RHI::AddressMode::ClampToEdge;
        SamplerDesc.AddressV   = RHI::AddressMode::ClampToEdge;
        SamplerDesc.DebugName  = "XEN.TAA";
        _Sampler               = Device.CreateSampler(SamplerDesc);

        if (!_Pipeline.IsValid() || !_Sampler.IsValid()) {
            Shutdown();
            return false;
        }
        return true;
    }

    void TAA::Shutdown() {
        if (!_Device) return;

        if (_Pipeline.IsValid()) _Device->DestroyPipeline(_Pipeline);
        if (_Layout.IsValid()) _Device->DestroyPipelineLayout(_Layout);
        if (_Sampler.IsValid()) _Device->DestroySampler(_Sampler);
        if (_History[0].IsValid()) _Device->DestroyTexture(_History[0]);
        if (_History[1].IsValid()) _Device->DestroyTexture(_History[1]);

        _Pipeline   = {};
        _Layout     = {};
        _Sampler    = {};
        _History[0] = _History[1] = {};
        _HistoryIndex  = 0;
        _HistoryPrimed = false;
        _Width = _Height = 0;
        _JitterIndex      = 0;
        _Device           = nullptr;
    }

    Float2 TAA::GetJitterOffset(const u32 Width, const u32 Height) {
        constexpr u32 SampleCount = 8;
        ++_JitterIndex;
        // 1-based: index 0 gives (0,0) for both bases, a degenerate
        // "no jitter" sample that's a worse use of the cycle than any other.
        const u32 HaltonIndex = (_JitterIndex % SampleCount) + 1;
        const f32 JitterX     = RadicalInverse(HaltonIndex, 2) - 0.5f;
        const f32 JitterY     = RadicalInverse(HaltonIndex, 3) - 0.5f;

        // Sub-pixel offset -> NDC: a whole pixel spans 2/Width (or Height)
        // of NDC, so a [-0.5, 0.5]-pixel offset is [-1, 1] * (1/Width).
        return {(2.0f * JitterX) / CAST<f32>(Width), (2.0f * JitterY) / CAST<f32>(Height)};
    }

    void TAA::EnsureHistoryTargets(const u32 Width, const u32 Height) {
        if (_History[0].IsValid() && _Width == Width && _Height == Height) return;

        if (_History[0].IsValid()) _Device->DestroyTexture(_History[0]);
        if (_History[1].IsValid()) _Device->DestroyTexture(_History[1]);

        RHI::TextureDesc Desc;
        Desc.Width     = Width;
        Desc.Height    = Height;
        Desc.Fmt       = _ColorFormat;
        Desc.Usage     = RHI::TextureUsage::ColorTarget | RHI::TextureUsage::Sampled;
        Desc.DebugName = "XEN.TAA.History0";
        _History[0]    = _Device->CreateTexture(Desc);
        Desc.DebugName = "XEN.TAA.History1";
        _History[1]    = _Device->CreateTexture(Desc);

        _Width        = Width;
        _Height       = Height;
        _HistoryIndex = 0;
        // The old history (if any) no longer matches this resolution -
        // treat this exactly like the very first frame.
        _HistoryPrimed = false;
    }

    RHI::TextureHandle TAA::Resolve(RHI::CommandBuffer& Commands,
                                    const RHI::TextureHandle SceneColor,
                                    const RHI::TextureHandle MotionVectors,
                                    const u32 Width,
                                    const u32 Height,
                                    const Settings& Settings_) {
        if (!_Device || !_Pipeline.IsValid() || !Settings_.Enabled) return SceneColor;

        EnsureHistoryTargets(Width, Height);
        if (!_History[0].IsValid() || !_History[1].IsValid()) return SceneColor;

        Commands.PushDebugGroup("TAA");

        const u32 PrevIndex = _HistoryIndex;
        const u32 NextIndex = PrevIndex ^ 1;

        const RHI::RenderPassDesc Pass = RHI::RenderPassDesc::ColorTarget(_History[NextIndex]);
        Commands.BeginRenderPass(Pass);
        Commands.BindPipeline(_Pipeline);

        TaaParams Params {};
        Params.Params[0] = Settings_.BlendFactor;
        Params.Params[1] = _HistoryPrimed ? 1.0f : 0.0f;
        Commands.BindUniformBuffer(0, _Device->AllocateUniform(Params));
        Commands.BindTexture(0, SceneColor, _Sampler);
        Commands.BindTexture(1, MotionVectors, _Sampler);
        Commands.BindTexture(2, _History[PrevIndex], _Sampler);
        Commands.Draw(3);
        Commands.EndRenderPass();

        Commands.PopDebugGroup();

        _HistoryIndex  = NextIndex;
        _HistoryPrimed = true;

        return _History[NextIndex];
    }
}  // namespace Xen
