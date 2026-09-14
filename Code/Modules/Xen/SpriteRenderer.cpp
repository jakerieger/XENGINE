//
// Created by Jake Rieger on 9/11/2026.
//

#include "SpriteRenderer.hpp"

#include <cstdio>

namespace Xen {
    namespace {
        // Embedded GLSL keeps the first cut buildable with no asset pipeline.
        // When you set up an offline glslang step, switch ShaderSourceType to
        // SPIRV and load the blobs - GL 4.6 takes SPIR-V natively, and the
        // identical bytes feed a Vulkan backend later.

        constexpr auto SpriteVertexShader = R"(#version 460 core

layout(location = 0) in vec2 InCenter;
layout(location = 1) in vec2 InSize;
layout(location = 2) in float InRotation;
layout(location = 3) in vec4 InUVRect;
layout(location = 4) in vec4 InTint;

layout(std140, binding = 0) uniform FrameData {
    mat4 ViewProjection;
};

out vec2 vUV;
out vec4 vTint;

// Triangle strip order: BL, BR, TL, TR.
const vec2 Corners[4] = vec2[4](
    vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(-0.5, 0.5), vec2(0.5, 0.5)
);

// v flipped against the corner y, because stb_image loads top-down while
// world space is y-up. Sprite top must sample the first row of the image.
const vec2 CornerUVs[4] = vec2[4](
    vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 0.0)
);

void main() {
    vec2 Local = Corners[gl_VertexID] * InSize;

    float C = cos(InRotation);
    float S = sin(InRotation);
    vec2 Rotated = vec2(Local.x * C - Local.y * S, Local.x * S + Local.y * C);

    vUV   = InUVRect.xy + CornerUVs[gl_VertexID] * InUVRect.zw;
    vTint = InTint;

    gl_Position = ViewProjection * vec4(InCenter + Rotated, 0.0, 1.0);
}
)";

        constexpr auto SpriteFragmentShader = R"(#version 460 core

in vec2 vUV;
in vec4 vTint;

layout(binding = 0) uniform sampler2D Albedo;

out vec4 FragColor;

void main() {
    vec4 Color = texture(Albedo, vUV) * vTint;
    if (Color.a <= 0.0) discard;
    FragColor = Color;
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
        VertexDesc.SourceType = RHI::ShaderSourceType::GLSL;
        VertexDesc.Code       = SpriteVertexShader;
        VertexDesc.CodeSize   = std::strlen(SpriteVertexShader);
        VertexDesc.DebugName  = "Sprite.vert";

        RHI::ShaderDesc FragmentDesc;
        FragmentDesc.Stage      = RHI::ShaderStage::Fragment;
        FragmentDesc.SourceType = RHI::ShaderSourceType::GLSL;
        FragmentDesc.Code       = SpriteFragmentShader;
        FragmentDesc.CodeSize   = std::strlen(SpriteFragmentShader);
        FragmentDesc.DebugName  = "Sprite.frag";

        const RHI::ShaderHandle Vertex   = Device.CreateShader(VertexDesc);
        const RHI::ShaderHandle Fragment = Device.CreateShader(FragmentDesc);
        if (!Vertex.IsValid() || !Fragment.IsValid()) {
            _Device = nullptr;
            return false;
        }

        RHI::GraphicsPipelineDesc PipelineDesc;
        PipelineDesc.VertexShader   = Vertex;
        PipelineDesc.FragmentShader = Fragment;
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
        PipelineDesc.ColorFormats[0]               = RHI::Format::RGBA8_UNORM;
        PipelineDesc.DebugName                     = "Sprite";

        _Pipeline = Device.CreateGraphicsPipeline(PipelineDesc);

        // The shader objects are no longer needed once the program is linked.
        Device.DestroyShader(Vertex);
        Device.DestroyShader(Fragment);

        if (!_Pipeline.IsValid()) {
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
        if (_Sampler.IsValid()) _Device->DestroySampler(_Sampler);

        _Pipeline = {};
        _Sampler  = {};
        _Device   = nullptr;
    }

    void SpriteRenderer::Render(const SpriteBatcher& Batcher, const TextureCache& Textures) {
        if (!_Device) return;

        _SpritesSubmitted = 0;
        _SpritesDropped   = 0;

        _Commands.Reset();
        _Commands.PushDebugGroup("Sprites");

        RHI::RenderPassDesc Pass = RHI::RenderPassDesc::SwapChain(_Config.ClearColor.r,
                                                                  _Config.ClearColor.g,
                                                                  _Config.ClearColor.b,
                                                                  _Config.ClearColor.a);
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
                    Center                                             = Item.WorldTransform.Position;
                    Size     = glm::vec2 {Item.SourceRect.Width / PixelsPerUnit * Item.WorldTransform.Scale.x,
                                      Item.SourceRect.Height / PixelsPerUnit * Item.WorldTransform.Scale.y};
                    Rotation = Item.WorldTransform.Rotation;
                    _Pad     = 0.0f;
                    UVRect   = glm::vec4 {Item.SourceRect.X / TexWidth,
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