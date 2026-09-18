//
// Created by Jake Rieger on 9/11/2026.
//

#include "SpriteRenderer.hpp"

#include <cstdio>

namespace Xen {
    namespace {
        // Kept identical to Shaders/HLSL/Sprite.hlsl (which carries the fully
        // commented version) - embedded here rather than loaded from disk so
        // the demo has no asset pipeline dependency. D3D12RenderDevice::
        // CreateShader compiles this at creation time via the legacy
        // D3DCompile (SM 5.x), the same "author a shader, run the demo"
        // workflow the old runtime-compiled GLSL had.
        constexpr auto SpriteShaderSource = R"(
cbuffer FrameData : register(b0) {
    row_major float4x4 ViewProjection;
};

Texture2D Albedo : register(t0);
SamplerState AlbedoSampler : register(s0);

struct VSInput {
    float2 Center   : TEXCOORD0;
    float2 Size     : TEXCOORD1;
    float Rotation  : TEXCOORD2;
    float4 UVRect   : TEXCOORD3;
    float4 Tint     : TEXCOORD4;
    uint VertexID   : SV_VertexID;
};

struct PSInput {
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
    float4 Tint : COLOR0;
};

static const float2 Corners[4] = {
  float2(-0.5, -0.5), float2(0.5, -0.5), float2(-0.5, 0.5), float2(0.5, 0.5)
};

static const float2 CornerUVs[4] = {
  float2(0.0, 1.0), float2(1.0, 1.0), float2(0.0, 0.0), float2(1.0, 0.0)
};

PSInput VSMain(VSInput In) {
    const float2 Local = Corners[In.VertexID] * In.Size;

    const float C = cos(In.Rotation);
    const float S = sin(In.Rotation);
    const float2 Rotated = float2(Local.x * C - Local.y * S, Local.x * S + Local.y * C);

    PSInput Out;
    Out.UV   = In.UVRect.xy + CornerUVs[In.VertexID] * In.UVRect.zw;
    Out.Tint = In.Tint;

    const float2 WorldPos = In.Center + Rotated;
    Out.Position          = mul(float4(WorldPos, 0.0, 1.0), ViewProjection);

    return Out;
}

float4 PSMain(PSInput In) : SV_Target {
    const float4 Color = Albedo.Sample(AlbedoSampler, In.UV) * In.Tint;
    clip(Color.a <= 0.0 ? -1.0 : 1.0);
    return Color;
}
)";
    }  // namespace

    SpriteRenderer::~SpriteRenderer() {
        Shutdown();
    }

    bool SpriteRenderer::Initialize(RHI::IRenderDevice& Device, const Config& Cfg) {
        _Device = &Device;
        _Config = Cfg;

        RHI::ShaderDesc VertexDesc;
        VertexDesc.Stage      = RHI::ShaderStage::Vertex;
        VertexDesc.SourceType = RHI::ShaderSourceType::HLSL;
        VertexDesc.Code       = SpriteShaderSource;
        VertexDesc.CodeSize   = std::strlen(SpriteShaderSource);
        VertexDesc.EntryPoint = "VSMain";
        VertexDesc.DebugName  = "Sprite.vs";

        RHI::ShaderDesc FragmentDesc;
        FragmentDesc.Stage      = RHI::ShaderStage::Fragment;
        FragmentDesc.SourceType = RHI::ShaderSourceType::HLSL;
        FragmentDesc.Code       = SpriteShaderSource;
        FragmentDesc.CodeSize   = std::strlen(SpriteShaderSource);
        FragmentDesc.EntryPoint = "PSMain";
        FragmentDesc.DebugName  = "Sprite.ps";

        const RHI::ShaderHandle Vertex   = Device.CreateShader(VertexDesc);
        const RHI::ShaderHandle Fragment = Device.CreateShader(FragmentDesc);
        if (!Vertex.IsValid() || !Fragment.IsValid()) {
            _Device = nullptr;
            return false;
        }

        // b0 uniform buffer (per-frame view-projection), t0/s0 sprite texture
        // and its sampler - matches SpriteShaderSource's register() decls
        // above exactly. Slot 0 is shared between the SampledTexture and
        // Sampler bindings on purpose: BindTexture resolves both through one
        // Slot value, the same way the HLSL pairs t0 with s0.
        RHI::PipelineLayoutDesc LayoutDesc;
        LayoutDesc.Binding(0, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::All)
          .Binding(0, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Fragment)
          .Binding(0, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment);
        LayoutDesc.DebugName = "Sprite";

        _Layout = Device.CreatePipelineLayout(LayoutDesc);
        if (!_Layout.IsValid()) {
            Device.DestroyShader(Vertex);
            Device.DestroyShader(Fragment);
            _Device = nullptr;
            return false;
        }

        RHI::GraphicsPipelineDesc PipelineDesc;
        PipelineDesc.VertexShader   = Vertex;
        PipelineDesc.FragmentShader = Fragment;
        PipelineDesc.PipelineLayout = _Layout;
        PipelineDesc.Topology       = RHI::PrimitiveTopology::TriangleStrip;

        // A single instance-rate binding. No per-vertex data at all: the four
        // corners are generated from gl_VertexID.
        PipelineDesc.Layout.Binding(0, sizeof(SpriteInstance), RHI::VertexInputRate::Instance)
          .Attribute(0, 0, RHI::Format::RG32_FLOAT, offsetof(SpriteInstance, Center))
          .Attribute(1, 0, RHI::Format::RG32_FLOAT, offsetof(SpriteInstance, Size))
          .Attribute(2, 0, RHI::Format::R32_FLOAT, offsetof(SpriteInstance, Rotation))
          .Attribute(3, 0, RHI::Format::RGBA32_FLOAT, offsetof(SpriteInstance, UVRect))
          .Attribute(4, 0, RHI::Format::RGBA32_FLOAT, offsetof(SpriteInstance, Tint));

        // No culling: a negative scale flips the winding, and mirrored sprites
        // are routine. No depth: SpriteBatcher already sorted by layer on the
        // CPU, and a depth test would break alpha blending between layers.
        PipelineDesc.Rasterizer.Cull               = RHI::CullMode::None;
        PipelineDesc.DepthStencil.DepthTestEnable  = false;
        PipelineDesc.DepthStencil.DepthWriteEnable = false;
        PipelineDesc.Blend.Attachments[0]          = RHI::BlendAttachmentState::AlphaBlend();
        PipelineDesc.ColorAttachmentCount          = 1;
        // Must match Viewport's own color format (see Viewport::Initialize) -
        // both this PSO's declared render-target format and the actual
        // attachment it draws into need to agree.
        PipelineDesc.ColorFormats[0] = RHI::Format::BGRA8_UNORM;
        PipelineDesc.DebugName       = "Sprite";

        _Pipeline = Device.CreateGraphicsPipeline(PipelineDesc);

        // The shader objects are no longer needed once the program is linked.
        Device.DestroyShader(Vertex);
        Device.DestroyShader(Fragment);

        if (!_Pipeline.IsValid()) {
            Device.DestroyPipelineLayout(_Layout);
            _Layout = {};
            _Device = nullptr;
            return false;
        }

        RHI::SamplerDesc SamplerDesc;
        SamplerDesc.MinFilter = Cfg.Filter;
        SamplerDesc.MagFilter = Cfg.Filter;
        SamplerDesc.MipFilter = RHI::MipMode::None;
        // ClampToEdge rather than Repeat: an atlas sub-rect sampled with
        // Repeat wraps to the far side of the sheet at the seams.
        SamplerDesc.AddressU  = RHI::AddressMode::ClampToEdge;
        SamplerDesc.AddressV  = RHI::AddressMode::ClampToEdge;
        SamplerDesc.DebugName = "Sprite";

        _Sampler = Device.CreateSampler(SamplerDesc);
        return _Sampler.IsValid();
    }

    void SpriteRenderer::Shutdown() {
        if (!_Device) return;

        if (_Pipeline.IsValid()) _Device->DestroyPipeline(_Pipeline);
        if (_Layout.IsValid()) _Device->DestroyPipelineLayout(_Layout);
        if (_Sampler.IsValid()) _Device->DestroySampler(_Sampler);

        _Pipeline = {};
        _Layout   = {};
        _Sampler  = {};
        _Device   = nullptr;
    }

    void SpriteRenderer::Render(const SpriteBatcher& Batcher, const TextureCache& Textures, const Viewport& Target) {
        if (!_Device) return;

        _SpritesSubmitted = 0;
        _SpritesDropped   = 0;

        _Commands.Reset();
        _Commands.PushDebugGroup("Sprites");

        RHI::RenderPassDesc Pass = RHI::RenderPassDesc::ColorTarget(Target.GetColorTarget(),
                                                                    _Config.ClearColor.x,
                                                                    _Config.ClearColor.y,
                                                                    _Config.ClearColor.z,
                                                                    _Config.ClearColor.w);
        Pass.DebugName           = "Sprites";
        _Commands.BeginRenderPass(Pass);

        const std::vector<SpriteDrawItem>& Items = Batcher.GetItems();
        const std::vector<SpriteBatch>& Batches  = Batcher.GetBatches();

        if (!Items.empty()) {
            u32 Count = CAST<u32>(Items.size());
            if (Count > _Config.MaxSpritesPerFrame) {
                _SpritesDropped = Count - _Config.MaxSpritesPerFrame;
                Count           = _Config.MaxSpritesPerFrame;
                std::fprintf(stderr,
                             "[Xen::SpriteRenderer] %u sprites over the per-frame cap were dropped\n",
                             _SpritesDropped);
            }

            const RHI::TransientAllocation Instances =
              _Device->AllocateTransient(Count * sizeof(SpriteInstance), RHI::BufferUsage::Vertex);

            if (Instances.IsValid()) {
                // PixelsPerUnit converts a source rect in texels into world
                // units. Without a camera SpriteBatcher leaves the view
                // matrix at identity, so fall back to the same 100 it uses.
                const CameraComponent* Camera = Batcher.GetActiveCamera();
                const f32 PixelsPerUnit       = Camera ? Camera->GetPixelsPerUnit() : 100.0f;

                auto* Out = CAST<SpriteInstance*>(Instances.Data);

                for (u32 i = 0; i < Count; ++i) {
                    const SpriteDrawItem& Item = Items[i];
                    const auto [Width, Height] = Textures.GetInfo(Item.Texture);

                    // A texture with no recorded size would divide by zero
                    // below. Emit a degenerate quad instead of NaNs, which
                    // are far harder to trace back.
                    const f32 TexWidth  = Width ? CAST<f32>(Width) : 1.0f;
                    const f32 TexHeight = Height ? CAST<f32>(Height) : 1.0f;

                    auto& [Center, Size, Rotation, _Pad, UVRect, Tint] = Out[i];
                    // 2D-only: the GPU instance format is a flat Float2, so
                    // Position.z/Scale.z never reach the sprite pipeline.
                    Center   = Float2 {Item.WorldTransform.Position.x, Item.WorldTransform.Position.y};
                    Size     = Float2 {Item.SourceRect.Width / PixelsPerUnit * Item.WorldTransform.Scale.x,
                                    Item.SourceRect.Height / PixelsPerUnit * Item.WorldTransform.Scale.y};
                    Rotation = Item.WorldTransform.GetRotationZ();
                    _Pad     = 0.0f;
                    UVRect   = Float4 {Item.SourceRect.X / TexWidth,
                                    Item.SourceRect.Y / TexHeight,
                                    Item.SourceRect.Width / TexWidth,
                                    Item.SourceRect.Height / TexHeight};
                    Tint     = Item.Tint;
                }

                FrameUniforms Uniforms {};
                Uniforms.ViewProjection = Batcher.GetViewProjection();

                _Commands.BindPipeline(_Pipeline);
                _Commands.BindUniformBuffer(0, _Device->AllocateUniform(Uniforms));
                _Commands.BindVertexBuffer(0, Instances);

                // One texture bind and one draw per batch. FirstInstance lets
                // every batch read from the same buffer at its own offset, so
                // there is exactly one upload for the whole frame.
                for (const auto& [Texture, FirstItem, ItemCount] : Batches) {
                    const u32 First = CAST<u32>(FirstItem);
                    if (First >= Count) break;

                    u32 InstanceCount = CAST<u32>(ItemCount);
                    if (First + InstanceCount > Count) InstanceCount = Count - First;
                    if (InstanceCount == 0) continue;

                    _Commands.BindTexture(0, Texture, _Sampler);
                    _Commands.Draw(4, InstanceCount, 0, First);
                    _SpritesSubmitted += InstanceCount;
                }
            }
        }

        _Commands.EndRenderPass();
        _Commands.PopDebugGroup();

        _Device->Submit(_Commands);
    }
}  // namespace Xen