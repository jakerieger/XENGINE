//
// Created by Jake Rieger on 9/17/2026.
//

#include "MeshRenderer.hpp"
#include "MaterialBindings.hpp"
#include "MeshComponent.hpp"
#include "PBRMaterialComponent.hpp"
#include "DirectionalLightComponent.hpp"
#include "EnvironmentComponent.hpp"
#include "PostProcessComponent.hpp"
#include "CameraComponent.hpp"
#include "Actor.hpp"
#include "Scene.hpp"

#include <XenPAK/AssetRegistry.hpp>
#include <XenPAK/AssetBuffer.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace Xen {
    namespace {
        // Matches Code/Shaders/Include/FrameData.hlsli exactly.
        struct FrameConstants {
            Float4x4 ViewProjection;
            Float4x4 InvViewProjection;
            Float4 CameraPositionAndPad;
            Float4 LightDirectionAndPad;
            Float4 LightColorAndIntensity;
            Float4x4 LightViewProjection;
            Float4 ShadowParams;
            Float4 ShadowParams2;
        };

        struct ObjectConstants {
            Float4x4 Model;
        };

        struct MaterialConstants {
            Float4 AlbedoAndMetallic;
            Float4 RoughnessAOAndPad;
            Float4 EmissiveAndPad;
        };

        // The scene render's own target format (see MeshRenderer.hpp's
        // _SceneColorTarget) - fixed, unlike TargetFormats.GetColorFormat(),
        // which only ever governs the post-process composite pipeline now.
        constexpr RHI::Format SceneColorFormat = RHI::Format::RGBA16_FLOAT;
    }  // namespace

    MeshRenderer::~MeshRenderer() {
        Shutdown();
    }

    bool MeshRenderer::Initialize(RHI::IRenderDevice& Device,
                                  const PAK::AssetRegistry& Assets,
                                  const Viewport& TargetFormats) {
        _Device = &Device;

        // Precompiled DXIL only - never HLSL source from a game's own
        // Content directory. Code/Shaders/PBR.hlsl is the authored source;
        // Scripts/compile_engine_shaders.py compiles it offline (dxc.exe) to
        // Engine/Shaders/pbr.vs + pbr.ps, and those get packed into
        // Engine/XEN.Shaders.xpak, the only place this ever loads them from.
        constexpr AssetID VertexAsset   = ASSET("xen.shader.pbr.vs");
        constexpr AssetID FragmentAsset = ASSET("xen.shader.pbr.ps");
        if (!Assets.Contains(VertexAsset) || !Assets.Contains(FragmentAsset)) {
            _Device = nullptr;
            return false;
        }
        const PAK::AssetBuffer VertexSource   = Assets.Load(VertexAsset);
        const PAK::AssetBuffer FragmentSource = Assets.Load(FragmentAsset);

        RHI::ShaderDesc VertexDesc;
        VertexDesc.Stage      = RHI::ShaderStage::Vertex;
        VertexDesc.SourceType = RHI::ShaderSourceType::DXIL;
        VertexDesc.Code       = VertexSource.Data();
        VertexDesc.CodeSize   = VertexSource.Size();
        VertexDesc.DebugName  = "XEN.Shaders.PBR.vs";

        RHI::ShaderDesc FragmentDesc;
        FragmentDesc.Stage      = RHI::ShaderStage::Fragment;
        FragmentDesc.SourceType = RHI::ShaderSourceType::DXIL;
        FragmentDesc.Code       = FragmentSource.Data();
        FragmentDesc.CodeSize   = FragmentSource.Size();
        FragmentDesc.DebugName  = "XEN.Shaders.PBR.ps";

        const RHI::ShaderHandle Vertex   = Device.CreateShader(VertexDesc);
        const RHI::ShaderHandle Fragment = Device.CreateShader(FragmentDesc);
        if (!Vertex.IsValid() || !Fragment.IsValid()) {
            _Device = nullptr;
            return false;
        }

        // b0 per-frame (view-projection, camera, light), b1 per-object
        // (model matrix), b2 per-material (constant PBR factors), plus the
        // texture+sampler pairs at the standardized slots every material
        // pipeline built this way shares - see MaterialBindings.hpp and
        // Code/Shaders/Include/MaterialBindings.hlsli, which this must match
        // exactly.
        RHI::PipelineLayoutDesc LayoutDesc;
        LayoutDesc.Binding(MaterialSlot::Frame, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::All)
          .Binding(MaterialSlot::Object, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::All)
          .Binding(MaterialSlot::Material, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::Fragment);
        for (u32 Slot = 0; Slot < MaterialSlot::TextureSlotCount; ++Slot) {
            LayoutDesc.Binding(Slot, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Fragment)
              .Binding(Slot, RHI::BindingType::Sampler, RHI::ShaderVisibility::Fragment);
        }
        LayoutDesc.DebugName = "XEN.Shaders.PBR";

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
        PipelineDesc.Topology       = RHI::PrimitiveTopology::TriangleList;

        // Matches MeshVertex exactly (see MeshCache.hpp): Position, Normal,
        // Tangent, UV, one per-vertex (not instanced) binding.
        PipelineDesc.Layout.Binding(0, sizeof(MeshVertex), RHI::VertexInputRate::Vertex)
          .Attribute(0, 0, RHI::Format::RGB32_FLOAT, offsetof(MeshVertex, Position))
          .Attribute(1, 0, RHI::Format::RGB32_FLOAT, offsetof(MeshVertex, Normal))
          .Attribute(2, 0, RHI::Format::RGB32_FLOAT, offsetof(MeshVertex, Tangent))
          .Attribute(3, 0, RHI::Format::RG32_FLOAT, offsetof(MeshVertex, UV));

        PipelineDesc.Rasterizer.Cull               = RHI::CullMode::Back;
        PipelineDesc.DepthStencil.DepthTestEnable  = true;
        PipelineDesc.DepthStencil.DepthWriteEnable = true;
        PipelineDesc.DepthStencil.DepthCompare     = RHI::CompareOp::Less;
        PipelineDesc.ColorAttachmentCount          = 1;
        PipelineDesc.ColorFormats[0]               = SceneColorFormat;
        PipelineDesc.DepthFormat                   = TargetFormats.GetDepthFormat();
        PipelineDesc.DebugName                     = "XEN.Shaders.PBR";

        _Pipeline = Device.CreateGraphicsPipeline(PipelineDesc);

        // The environment background: same layout (so every binding is
        // already in place when Render switches to it), drawn at the far
        // plane so it only shows where no mesh is - depth-tested LessEqual
        // against a buffer cleared to 1.0, never written. Optional: a missing
        // shader just means no background, not no mesh rendering.
        {
            constexpr AssetID SkyVertexAsset   = ASSET("xen.shader.sky.vs");
            constexpr AssetID SkyFragmentAsset = ASSET("xen.shader.sky.ps");
            if (Assets.Contains(SkyVertexAsset) && Assets.Contains(SkyFragmentAsset)) {
                const PAK::AssetBuffer SkyVertexSource   = Assets.Load(SkyVertexAsset);
                const PAK::AssetBuffer SkyFragmentSource = Assets.Load(SkyFragmentAsset);

                RHI::ShaderDesc SkyVertexDesc;
                SkyVertexDesc.Stage      = RHI::ShaderStage::Vertex;
                SkyVertexDesc.SourceType = RHI::ShaderSourceType::DXIL;
                SkyVertexDesc.Code       = SkyVertexSource.Data();
                SkyVertexDesc.CodeSize   = SkyVertexSource.Size();
                SkyVertexDesc.DebugName  = "XEN.Shaders.Sky.vs";

                RHI::ShaderDesc SkyFragmentDesc;
                SkyFragmentDesc.Stage      = RHI::ShaderStage::Fragment;
                SkyFragmentDesc.SourceType = RHI::ShaderSourceType::DXIL;
                SkyFragmentDesc.Code       = SkyFragmentSource.Data();
                SkyFragmentDesc.CodeSize   = SkyFragmentSource.Size();
                SkyFragmentDesc.DebugName  = "XEN.Shaders.Sky.ps";

                const RHI::ShaderHandle SkyVertex   = Device.CreateShader(SkyVertexDesc);
                const RHI::ShaderHandle SkyFragment = Device.CreateShader(SkyFragmentDesc);

                if (SkyVertex.IsValid() && SkyFragment.IsValid()) {
                    RHI::GraphicsPipelineDesc SkyDesc;
                    SkyDesc.VertexShader                    = SkyVertex;
                    SkyDesc.FragmentShader                  = SkyFragment;
                    SkyDesc.PipelineLayout                  = _Layout;
                    SkyDesc.Topology                        = RHI::PrimitiveTopology::TriangleList;
                    SkyDesc.Rasterizer.Cull                 = RHI::CullMode::None;
                    SkyDesc.DepthStencil.DepthTestEnable    = true;
                    SkyDesc.DepthStencil.DepthWriteEnable   = false;
                    SkyDesc.DepthStencil.DepthCompare       = RHI::CompareOp::LessEqual;
                    SkyDesc.ColorAttachmentCount            = 1;
                    SkyDesc.ColorFormats[0]                 = SceneColorFormat;
                    SkyDesc.DepthFormat                     = TargetFormats.GetDepthFormat();
                    SkyDesc.DebugName                       = "XEN.Shaders.Sky";
                    _SkyPipeline                            = Device.CreateGraphicsPipeline(SkyDesc);
                }

                if (SkyVertex.IsValid()) Device.DestroyShader(SkyVertex);
                if (SkyFragment.IsValid()) Device.DestroyShader(SkyFragment);
            }
        }

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
        SamplerDesc.DebugName = "XEN.PBR";
        _Sampler              = Device.CreateSampler(SamplerDesc);

        RHI::SamplerDesc EnvironmentSamplerDesc;
        EnvironmentSamplerDesc.AddressU  = RHI::AddressMode::Repeat;
        EnvironmentSamplerDesc.AddressV  = RHI::AddressMode::ClampToEdge;
        EnvironmentSamplerDesc.DebugName = "XEN.PBR.Environment";
        _EnvironmentSampler              = Device.CreateSampler(EnvironmentSamplerDesc);

        RHI::SamplerDesc ClampSamplerDesc;
        ClampSamplerDesc.AddressU  = RHI::AddressMode::ClampToEdge;
        ClampSamplerDesc.AddressV  = RHI::AddressMode::ClampToEdge;
        ClampSamplerDesc.DebugName = "XEN.PBR.Clamp";
        _ClampSampler              = Device.CreateSampler(ClampSamplerDesc);

        // Clamped, not bordered: the shader already treats anything outside
        // the map's footprint as lit, so an edge texel is never sampled for
        // its own sake.
        RHI::SamplerDesc ShadowSamplerDesc;
        ShadowSamplerDesc.MipFilter   = RHI::MipMode::None;
        ShadowSamplerDesc.AddressU    = RHI::AddressMode::ClampToEdge;
        ShadowSamplerDesc.AddressV    = RHI::AddressMode::ClampToEdge;
        ShadowSamplerDesc.Compare     = true;
        ShadowSamplerDesc.CompareFunc = RHI::CompareOp::LessEqual;
        ShadowSamplerDesc.DebugName   = "XEN.PBR.Shadow";
        _ShadowSampler                = Device.CreateSampler(ShadowSamplerDesc);

        if (!_Sampler.IsValid() || !_EnvironmentSampler.IsValid() || !_ClampSampler.IsValid() ||
            !_ShadowSampler.IsValid() || !CreateDefaultTextures() || !BakeBrdfLut(Assets)) {
            Shutdown();
            return false;
        }

        CreateShadowPipeline(Assets);

        // Not fatal: without the baker a scene's environment can't be
        // prefiltered, so it just keeps the placeholder sky (Render checks
        // IsInitialized before trying to bake).
        _Baker.Initialize(Device, Assets);

        // Fatal: without it there's no path from the scene's linear HDR
        // render to anything a swap chain can present.
        if (!_PostProcess.Initialize(Device, Assets, TargetFormats.GetColorFormat())) {
            Shutdown();
            return false;
        }

        return true;
    }

    void MeshRenderer::ReleaseBakedEnvironment() {
        if (_PrefilteredEnvironment.IsValid()) _Device->DestroyTexture(_PrefilteredEnvironment);
        if (_IrradianceMap.IsValid()) _Device->DestroyTexture(_IrradianceMap);

        _PrefilteredEnvironment = {};
        _IrradianceMap          = {};
        _BakedSource            = {};
    }

    bool MeshRenderer::BakeBrdfLut(const PAK::AssetRegistry& Assets) {
        constexpr AssetID VertexAsset   = ASSET("xen.shader.brdfintegrate.vs");
        constexpr AssetID FragmentAsset = ASSET("xen.shader.brdfintegrate.ps");
        if (!Assets.Contains(VertexAsset) || !Assets.Contains(FragmentAsset)) return false;

        const PAK::AssetBuffer VertexSource   = Assets.Load(VertexAsset);
        const PAK::AssetBuffer FragmentSource = Assets.Load(FragmentAsset);

        RHI::ShaderDesc VertexDesc;
        VertexDesc.Stage      = RHI::ShaderStage::Vertex;
        VertexDesc.SourceType = RHI::ShaderSourceType::DXIL;
        VertexDesc.Code       = VertexSource.Data();
        VertexDesc.CodeSize   = VertexSource.Size();
        VertexDesc.DebugName  = "XEN.Shaders.BRDFIntegrate.vs";

        RHI::ShaderDesc FragmentDesc;
        FragmentDesc.Stage      = RHI::ShaderStage::Fragment;
        FragmentDesc.SourceType = RHI::ShaderSourceType::DXIL;
        FragmentDesc.Code       = FragmentSource.Data();
        FragmentDesc.CodeSize   = FragmentSource.Size();
        FragmentDesc.DebugName  = "XEN.Shaders.BRDFIntegrate.ps";

        const RHI::ShaderHandle Vertex   = _Device->CreateShader(VertexDesc);
        const RHI::ShaderHandle Fragment = _Device->CreateShader(FragmentDesc);

        // Nothing to bind: the integration is a pure function of the pixel's
        // own position, and the fullscreen triangle comes from SV_VertexID
        // (no vertex buffer), so this is an empty layout and vertex layout.
        RHI::PipelineLayoutDesc LayoutDesc;
        LayoutDesc.DebugName           = "XEN.Shaders.BRDFIntegrate";
        const RHI::LayoutHandle Layout = _Device->CreatePipelineLayout(LayoutDesc);

        RHI::PipelineHandle Pipeline;
        if (Vertex.IsValid() && Fragment.IsValid() && Layout.IsValid()) {
            RHI::GraphicsPipelineDesc PipelineDesc;
            PipelineDesc.VertexShader         = Vertex;
            PipelineDesc.FragmentShader       = Fragment;
            PipelineDesc.PipelineLayout       = Layout;
            PipelineDesc.Topology             = RHI::PrimitiveTopology::TriangleList;
            PipelineDesc.ColorAttachmentCount = 1;
            PipelineDesc.ColorFormats[0]      = RHI::Format::RG16_FLOAT;
            PipelineDesc.DebugName            = "XEN.Shaders.BRDFIntegrate";
            Pipeline                          = _Device->CreateGraphicsPipeline(PipelineDesc);
        }

        if (Vertex.IsValid()) _Device->DestroyShader(Vertex);
        if (Fragment.IsValid()) _Device->DestroyShader(Fragment);

        RHI::TextureDesc LutDesc;
        LutDesc.Width     = 256;
        LutDesc.Height    = 256;
        LutDesc.Fmt       = RHI::Format::RG16_FLOAT;
        LutDesc.Usage     = RHI::TextureUsage::ColorTarget | RHI::TextureUsage::Sampled;
        LutDesc.DebugName = "XEN.PBR.BrdfLUT";
        _BrdfLUT          = _Device->CreateTexture(LutDesc);

        const bool CanBake = Pipeline.IsValid() && _BrdfLUT.IsValid();
        if (CanBake) {
            RHI::CommandBuffer Bake;
            RHI::RenderPassDesc Pass = RHI::RenderPassDesc::ColorTarget(_BrdfLUT, 0.0f, 0.0f, 0.0f, 0.0f);
            Pass.DebugName           = "BRDF LUT";
            Bake.BeginRenderPass(Pass);
            Bake.BindPipeline(Pipeline);
            Bake.Draw(3);
            Bake.EndRenderPass();

            // Blocks until the GPU is done - the LUT is sampled by the very
            // first frame, so there's no in-flight window to hide this in,
            // and it's a one-time cost at startup, not per frame.
            _Device->SubmitAndWait(Bake);
        }

        if (Pipeline.IsValid()) _Device->DestroyPipeline(Pipeline);
        if (Layout.IsValid()) _Device->DestroyPipelineLayout(Layout);

        return CanBake;
    }

    bool MeshRenderer::CreateDefaultTextures() {
        // Bound for any material channel with no map assigned (see
        // MeshRenderer.hpp) - both are 1x1 so the cost of always binding
        // them, for a game with no textured materials at all, is trivial.
        const auto MakeTexture = [this](const u32 Width, const u32 Height, const u8* Pixels, const char* Name) {
            RHI::TextureDesc Desc;
            Desc.Width     = Width;
            Desc.Height    = Height;
            Desc.Fmt       = RHI::Format::RGBA8_UNORM;
            Desc.Usage     = RHI::TextureUsage::Sampled;
            Desc.DebugName = Name;

            const RHI::TextureHandle Handle = _Device->CreateTexture(Desc);
            if (!Handle.IsValid()) return RHI::TextureHandle {};

            RHI::TextureUploadDesc Upload;
            Upload.Data     = Pixels;
            Upload.DataSize = CAST<size_t>(Width) * Height * 4;
            Upload.Width    = Width;
            Upload.Height   = Height;
            _Device->UploadTexture(Handle, Upload);

            return Handle;
        };

        const auto MakeSolid = [&](const u8 R, const u8 G, const u8 B, const u8 A, const char* Name) {
            const u8 Pixel[4] = {R, G, B, A};
            return MakeTexture(1, 1, Pixel, Name);
        };

        _WhiteTexture      = MakeSolid(255, 255, 255, 255, "XEN.PBR.White");
        _FlatNormalTexture = MakeSolid(128, 128, 255, 255, "XEN.PBR.FlatNormal");

        // A tiny placeholder sky cube: each face 4x4, +Y all sky, -Y all
        // ground, and the four side faces a top-to-bottom sky->ground
        // gradient (their top row is above the horizon, their bottom row
        // below it), so it reads as a horizon from any direction. Kept dim
        // and near-neutral on purpose: a strongly tinted stand-in competes
        // with a metal's own F0 tint. Sampled by direction like a real baked
        // cube, so the shader never branches on "is there an environment".
        {
            constexpr u32 FaceSize = 4;
            constexpr f32 Sky[3]    = {87.0f, 89.0f, 92.0f};
            constexpr f32 Ground[3] = {41.0f, 38.0f, 34.0f};

            RHI::TextureDesc Desc;
            Desc.Type        = RHI::TextureType::TextureCube;
            Desc.Width       = FaceSize;
            Desc.Height      = FaceSize;
            Desc.Fmt         = RHI::Format::RGBA8_UNORM;
            Desc.Usage       = RHI::TextureUsage::Sampled;
            Desc.DebugName   = "XEN.PBR.DefaultEnvironment";
            _DefaultEnvironmentMap = _Device->CreateTexture(Desc);

            if (_DefaultEnvironmentMap.IsValid()) {
                // D3D cube face order: +X, -X, +Y, -Y, +Z, -Z.
                for (u32 Face = 0; Face < 6; ++Face) {
                    u8 Pixels[FaceSize * FaceSize * 4];
                    for (u32 Row = 0; Row < FaceSize; ++Row) {
                        // 0 = all sky, 1 = all ground.
                        const f32 T = Face == 2 ? 0.0f : Face == 3 ? 1.0f : (CAST<f32>(Row) + 0.5f) / FaceSize;
                        for (u32 Col = 0; Col < FaceSize; ++Col) {
                            u8* Px = &Pixels[(Row * FaceSize + Col) * 4];
                            for (u32 C = 0; C < 3; ++C) Px[C] = CAST<u8>(Sky[C] + (Ground[C] - Sky[C]) * T);
                            Px[3] = 255;
                        }
                    }

                    RHI::TextureUploadDesc Upload;
                    Upload.Data       = Pixels;
                    Upload.DataSize   = sizeof(Pixels);
                    Upload.ArrayLayer = Face;
                    Upload.Width      = FaceSize;
                    Upload.Height     = FaceSize;
                    _Device->UploadTexture(_DefaultEnvironmentMap, Upload);
                }
            }
        }

        // The stand-in shadow map (see MeshRenderer.hpp): a depth texture is
        // created in DEPTH_WRITE and only reaches a samplable state by having
        // been rendered into once, so clear it to the far plane right now.
        {
            RHI::TextureDesc Desc;
            Desc.Fmt        = RHI::Format::D32_FLOAT;
            Desc.Usage      = RHI::TextureUsage::DepthTarget | RHI::TextureUsage::Sampled;
            Desc.DebugName  = "XEN.PBR.ShadowFallback";
            _ShadowFallback = _Device->CreateTexture(Desc);

            if (_ShadowFallback.IsValid()) {
                RHI::CommandBuffer Clear;
                RHI::RenderPassDesc Pass;
                Pass.HasDepthStencil          = true;
                Pass.DepthStencil.Texture     = _ShadowFallback;
                Pass.DepthStencil.DepthLoad   = RHI::LoadOp::Clear;
                Pass.DepthStencil.DepthStore  = RHI::StoreOp::Store;
                Pass.DepthStencil.Clear.Depth = 1.0f;
                Pass.DebugName                = "Shadow fallback";
                Clear.BeginRenderPass(Pass);
                Clear.EndRenderPass();
                _Device->SubmitAndWait(Clear);
            }
        }

        return _WhiteTexture.IsValid() && _FlatNormalTexture.IsValid() && _DefaultEnvironmentMap.IsValid() &&
               _ShadowFallback.IsValid();
    }

    void MeshRenderer::CreateShadowPipeline(const PAK::AssetRegistry& Assets) {
        constexpr AssetID VertexAsset = ASSET("xen.shader.shadow.vs");
        if (!Assets.Contains(VertexAsset)) {
            LOG_WARN("shadow shader not found - directional lights will not cast shadows");
            return;
        }
        const PAK::AssetBuffer VertexSource = Assets.Load(VertexAsset);

        RHI::ShaderDesc VertexDesc;
        VertexDesc.Stage               = RHI::ShaderStage::Vertex;
        VertexDesc.SourceType          = RHI::ShaderSourceType::DXIL;
        VertexDesc.Code                = VertexSource.Data();
        VertexDesc.CodeSize            = VertexSource.Size();
        VertexDesc.DebugName           = "XEN.Shaders.Shadow.vs";
        const RHI::ShaderHandle Vertex = _Device->CreateShader(VertexDesc);

        // Just the light's view-projection (b0, in FrameData's ViewProjection
        // slot) and the model matrix (b1): no textures, no material.
        RHI::PipelineLayoutDesc LayoutDesc;
        LayoutDesc.Binding(MaterialSlot::Frame, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::All)
          .Binding(MaterialSlot::Object, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::All);
        LayoutDesc.DebugName = "XEN.Shaders.Shadow";
        _ShadowLayout        = _Device->CreatePipelineLayout(LayoutDesc);

        if (Vertex.IsValid() && _ShadowLayout.IsValid()) {
            RHI::GraphicsPipelineDesc PipelineDesc;
            PipelineDesc.VertexShader   = Vertex;  // no fragment shader: depth only
            PipelineDesc.PipelineLayout = _ShadowLayout;
            PipelineDesc.Topology       = RHI::PrimitiveTopology::TriangleList;
            PipelineDesc.Layout.Binding(0, sizeof(MeshVertex), RHI::VertexInputRate::Vertex)
              .Attribute(0, 0, RHI::Format::RGB32_FLOAT, offsetof(MeshVertex, Position));

            // Both faces: the receiver-side normal offset already handles
            // self-shadowing, and culling either face would make a
            // one-sided plane (a ground quad) stop casting from below.
            PipelineDesc.Rasterizer.Cull               = RHI::CullMode::None;
            PipelineDesc.DepthStencil.DepthTestEnable  = true;
            PipelineDesc.DepthStencil.DepthWriteEnable = true;
            PipelineDesc.DepthStencil.DepthCompare     = RHI::CompareOp::Less;
            PipelineDesc.ColorAttachmentCount          = 0;
            PipelineDesc.DepthFormat                   = RHI::Format::D32_FLOAT;
            PipelineDesc.DebugName                     = "XEN.Shaders.Shadow";
            _ShadowPipeline                            = _Device->CreateGraphicsPipeline(PipelineDesc);
        }

        if (Vertex.IsValid()) _Device->DestroyShader(Vertex);

        if (!_ShadowPipeline.IsValid()) {
            LOG_WARN("failed to create the shadow pipeline - directional lights will not cast shadows");
            if (_ShadowLayout.IsValid()) _Device->DestroyPipelineLayout(_ShadowLayout);
            _ShadowLayout = {};
        }
    }

    void MeshRenderer::Shutdown() {
        if (!_Device) return;

        ReleaseBakedEnvironment();
        _Baker.Shutdown();
        _PostProcess.Shutdown();
        if (_SceneColorTarget.IsValid()) _Device->DestroyTexture(_SceneColorTarget);

        if (_ShadowPipeline.IsValid()) _Device->DestroyPipeline(_ShadowPipeline);
        if (_ShadowLayout.IsValid()) _Device->DestroyPipelineLayout(_ShadowLayout);
        if (_ShadowSampler.IsValid()) _Device->DestroySampler(_ShadowSampler);
        if (_ShadowMap.IsValid()) _Device->DestroyTexture(_ShadowMap);
        if (_ShadowFallback.IsValid()) _Device->DestroyTexture(_ShadowFallback);
        if (_SkyPipeline.IsValid()) _Device->DestroyPipeline(_SkyPipeline);
        if (_Pipeline.IsValid()) _Device->DestroyPipeline(_Pipeline);
        if (_Layout.IsValid()) _Device->DestroyPipelineLayout(_Layout);
        if (_Sampler.IsValid()) _Device->DestroySampler(_Sampler);
        if (_WhiteTexture.IsValid()) _Device->DestroyTexture(_WhiteTexture);
        if (_FlatNormalTexture.IsValid()) _Device->DestroyTexture(_FlatNormalTexture);
        if (_EnvironmentSampler.IsValid()) _Device->DestroySampler(_EnvironmentSampler);
        if (_ClampSampler.IsValid()) _Device->DestroySampler(_ClampSampler);
        if (_DefaultEnvironmentMap.IsValid()) _Device->DestroyTexture(_DefaultEnvironmentMap);
        if (_BrdfLUT.IsValid()) _Device->DestroyTexture(_BrdfLUT);

        _ShadowPipeline        = {};
        _ShadowLayout          = {};
        _ShadowSampler         = {};
        _ShadowMap             = {};
        _ShadowMapSize         = 0;
        _ShadowFallback        = {};
        _SkyPipeline           = {};
        _Pipeline              = {};
        _Layout                = {};
        _Sampler               = {};
        _WhiteTexture          = {};
        _FlatNormalTexture     = {};
        _EnvironmentSampler    = {};
        _ClampSampler          = {};
        _DefaultEnvironmentMap = {};
        _BrdfLUT               = {};
        _SceneColorTarget      = {};
        _SceneColorWidth       = 0;
        _SceneColorHeight      = 0;
        _Device                = nullptr;
    }

    void MeshRenderer::EnsureSceneColorTarget(const u32 Width, const u32 Height) {
        if (_SceneColorTarget.IsValid() && _SceneColorWidth == Width && _SceneColorHeight == Height) return;

        if (_SceneColorTarget.IsValid()) _Device->DestroyTexture(_SceneColorTarget);

        RHI::TextureDesc Desc;
        Desc.Width     = Width;
        Desc.Height    = Height;
        Desc.Fmt       = SceneColorFormat;
        Desc.Usage     = RHI::TextureUsage::ColorTarget | RHI::TextureUsage::Sampled;
        Desc.DebugName = "XEN.PBR.SceneColor";

        _SceneColorTarget = _Device->CreateTexture(Desc);
        _SceneColorWidth  = Width;
        _SceneColorHeight = Height;
    }

    MeshRenderer::EnvironmentState MeshRenderer::ResolveEnvironment(const Scene& S) {
        // The scene's first EnvironmentComponent with a map wins (same "first
        // one found" rule as the light); it's baked into the prefiltered/
        // irradiance pair the shader actually samples.
        RHI::TextureHandle EnvironmentSource {};
        u32 EnvironmentWidth   = 0;
        bool ShowBackground    = false;
        const std::vector<Actor*> Environments = S.FindActorsWith<EnvironmentComponent>();
        if (!Environments.empty()) {
            if (const auto* Env = Environments.front()->GetComponent<EnvironmentComponent>();
                Env && Env->GetMap().IsValid()) {
                EnvironmentSource = Env->GetMap();
                ShowBackground    = Env->GetShowBackground();
                if (const TextureCache* Textures = S.GetContext().Textures) {
                    EnvironmentWidth = Textures->GetInfo(EnvironmentSource).Width;
                }
            }
        }

        if (EnvironmentSource != _BakedSource) {
            // The baked pair belongs to a different (or no longer any)
            // environment - drop it before deciding whether to rebake.
            ReleaseBakedEnvironment();

            if (EnvironmentSource.IsValid() && _Baker.IsInitialized()) {
                // Recorded as attempted even if it fails, so a failing
                // bake isn't retried (and its textures re-created) every
                // frame - the placeholder sky just stays bound instead.
                _BakedSource = EnvironmentSource;

                EnvironmentBaker::Result Baked;
                if (_Baker.Bake(EnvironmentSource, EnvironmentWidth, Baked)) {
                    _PrefilteredEnvironment = Baked.Prefiltered;
                    _IrradianceMap          = Baked.Irradiance;
                }
            }
        }

        return {_PrefilteredEnvironment.IsValid() && _IrradianceMap.IsValid(), ShowBackground};
    }

    void MeshRenderer::PrepareEnvironment(const Scene& S) {
        if (_Device) ResolveEnvironment(S);
    }

    MeshRenderer::ShadowState MeshRenderer::RenderShadowPass(const Scene& S,
                                                             const CameraComponent& Camera,
                                                             const DirectionalLightComponent& Light,
                                                             const Float3& LightDirection,
                                                             MeshCache& Meshes) {
        using namespace DirectX;

        ShadowState Result;
        if (!_ShadowPipeline.IsValid() || !Light.GetCastShadows() ||
            Camera.GetProjectionMode() != ProjectionMode::Perspective) {
            return Result;
        }

        // The light's orientation only (eye at the origin): every quantity
        // below is measured in this space, and the final view-projection is
        // this times an orthographic projection whose window is offset to
        // where the camera's frustum actually is.
        const XMVECTOR Direction = XMVector3Normalize(XMLoadFloat3(&LightDirection));
        const XMVECTOR Up        = std::fabs(XMVectorGetY(Direction)) > 0.99f ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)
                                                                              : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMMATRIX LightRotation = XMMatrixLookToRH(XMVectorZero(), Direction, Up);

        // Casters: every mesh the main pass would draw. Their extent along
        // the light direction is what stretches the depth range back toward
        // the light - a tall object outside the view volume can still throw
        // its shadow into it.
        struct Caster {
            Float4x4 Model;
            RHI::BufferHandle Vertices;
            RHI::BufferHandle Indices;
            u32 IndexCount;
            RHI::IndexType IndexType;
        };
        std::vector<Caster> Casters;
        f32 NearestCasterDistance = std::numeric_limits<f32>::max();

        S.ForEachActor([&](Actor& A) {
            auto* MeshComp     = A.GetComponent<MeshComponent>();
            auto* MaterialComp = A.GetComponent<PBRMaterialComponent>();
            if (!MeshComp || !MaterialComp) return;

            const MeshHandle Mesh = MeshComp->GetMesh();
            if (!Mesh.IsValid()) return;

            const RHI::BufferHandle VertexBuffer = Meshes.GetVertexBuffer(Mesh);
            const RHI::BufferHandle IndexBuffer  = Meshes.GetIndexBuffer(Mesh);
            const MeshInfo Info                  = Meshes.GetInfo(Mesh);
            if (!VertexBuffer.IsValid() || !IndexBuffer.IsValid() || Info.IndexCount == 0) return;

            Caster C {};
            C.Model      = A.GetWorldTransform().ToMatrix();
            C.Vertices   = VertexBuffer;
            C.Indices    = IndexBuffer;
            C.IndexCount = Info.IndexCount;
            C.IndexType  = Info.IndexType;
            Casters.push_back(C);

            const XMMATRIX Model      = XMLoadFloat4x4(&C.Model);
            const XMMATRIX ToLight    = Model * LightRotation;
            for (u32 Corner = 0; Corner < 8; ++Corner) {
                const XMVECTOR Local = XMVectorSet((Corner & 1) ? Info.BoundsMax.x : Info.BoundsMin.x,
                                                   (Corner & 2) ? Info.BoundsMax.y : Info.BoundsMin.y,
                                                   (Corner & 4) ? Info.BoundsMax.z : Info.BoundsMin.z,
                                                   1.0f);
                // Light-space z is negative in front of the light; its
                // negation is the distance along the light direction.
                NearestCasterDistance =
                  std::min(NearestCasterDistance, -XMVectorGetZ(XMVector3Transform(Local, ToLight)));
            }
        });
        if (Casters.empty()) return Result;

        // The camera's view frustum out to the shadow distance, wrapped in
        // the tightest sphere (in view space, on the camera's axis). A sphere
        // rather than the frustum's own bounding box, so the map's size in
        // world units doesn't change as the camera turns - that, plus the
        // texel snapping below, keeps shadow edges from crawling.
        const f32 NearPlane   = Camera.GetNearPlane();
        const f32 FarDistance = std::min(Camera.GetFarPlane(), Light.GetShadowDistance());
        if (FarDistance <= NearPlane) return Result;

        const f32 TanHalfFov = std::tan(XMConvertToRadians(Camera.GetFieldOfView()) * 0.5f);
        const f32 K          = std::sqrt(1.0f + Camera.GetAspectRatio() * Camera.GetAspectRatio()) * TanHalfFov;

        f32 CenterDepth;  // distance in front of the camera
        f32 Radius;
        if (K * K >= (FarDistance - NearPlane) / (FarDistance + NearPlane)) {
            CenterDepth = FarDistance;
            Radius      = FarDistance * K;
        } else {
            CenterDepth = 0.5f * (FarDistance + NearPlane) * (1.0f + K * K);
            const f32 Span = FarDistance - NearPlane;
            const f32 Sum  = FarDistance + NearPlane;
            Radius         = 0.5f * std::sqrt(Span * Span + 2.0f * (FarDistance * FarDistance + NearPlane * NearPlane) * K * K +
                                              Sum * Sum * K * K * K * K);
        }
        Radius = std::ceil(Radius * 16.0f) / 16.0f;

        const Float4x4 ViewMatrix = Camera.GetViewMatrix();
        const XMMATRIX InverseView = XMMatrixInverse(nullptr, XMLoadFloat4x4(&ViewMatrix));
        const XMVECTOR CenterWorld = XMVector3Transform(XMVectorSet(0.0f, 0.0f, -CenterDepth, 1.0f), InverseView);
        XMVECTOR CenterLight       = XMVector3Transform(CenterWorld, LightRotation);

        const u32 Resolution = std::clamp<u32>(Light.GetShadowResolution(), 256, 8192);
        const f32 TexelSize  = 2.0f * Radius / CAST<f32>(Resolution);
        const f32 CenterX    = std::floor(XMVectorGetX(CenterLight) / TexelSize) * TexelSize;
        const f32 CenterY    = std::floor(XMVectorGetY(CenterLight) / TexelSize) * TexelSize;

        const f32 SphereCenterDistance = -XMVectorGetZ(CenterLight);
        const f32 NearDistance = std::min(SphereCenterDistance - Radius, NearestCasterDistance);
        const f32 FarShadowPlane = SphereCenterDistance + Radius;

        const XMMATRIX Projection =
          XMMatrixOrthographicOffCenterRH(CenterX - Radius, CenterX + Radius, CenterY - Radius, CenterY + Radius, NearDistance, FarShadowPlane);
        const XMMATRIX LightViewProjection = LightRotation * Projection;

        if (!_ShadowMap.IsValid() || _ShadowMapSize != Resolution) {
            if (_ShadowMap.IsValid()) _Device->DestroyTexture(_ShadowMap);

            RHI::TextureDesc Desc;
            Desc.Width     = Resolution;
            Desc.Height    = Resolution;
            Desc.Fmt       = RHI::Format::D32_FLOAT;
            Desc.Usage     = RHI::TextureUsage::DepthTarget | RHI::TextureUsage::Sampled;
            Desc.DebugName = "XEN.PBR.ShadowMap";
            _ShadowMap     = _Device->CreateTexture(Desc);
            _ShadowMapSize = _ShadowMap.IsValid() ? Resolution : 0;
        }
        if (!_ShadowMap.IsValid()) return Result;

        RHI::RenderPassDesc Pass;
        Pass.HasDepthStencil          = true;
        Pass.DepthStencil.Texture     = _ShadowMap;
        Pass.DepthStencil.DepthLoad   = RHI::LoadOp::Clear;
        Pass.DepthStencil.DepthStore  = RHI::StoreOp::Store;
        Pass.DepthStencil.Clear.Depth = 1.0f;
        Pass.DebugName                = "Shadow map";
        _Commands.BeginRenderPass(Pass);
        _Commands.BindPipeline(_ShadowPipeline);

        // Shadow.hlsl reads the light's matrix where the camera's normally
        // is, so the same constant-buffer layout serves both passes.
        FrameConstants ShadowFrame {};
        XMStoreFloat4x4(&ShadowFrame.ViewProjection, LightViewProjection);
        _Commands.BindUniformBuffer(MaterialSlot::Frame, _Device->AllocateUniform(ShadowFrame));

        for (const Caster& C : Casters) {
            ObjectConstants Object {};
            Object.Model = C.Model;
            _Commands.BindUniformBuffer(MaterialSlot::Object, _Device->AllocateUniform(Object));
            _Commands.BindVertexBuffer(0, C.Vertices);
            _Commands.BindIndexBuffer(C.Indices, C.IndexType);
            _Commands.DrawIndexed(C.IndexCount);
        }

        _Commands.EndRenderPass();

        // Bias is authored in texels of the map (see DirectionalLightComponent)
        // and converted here: the constant one to light-NDC depth, the normal
        // offset to world units.
        const f32 DepthRange = std::max(FarShadowPlane - NearDistance, 0.001f);
        Result.Enabled       = true;
        XMStoreFloat4x4(&Result.LightViewProjection, LightViewProjection);
        Result.Params  = {1.0f,
                          Light.GetShadowBias() * TexelSize / DepthRange,
                          Light.GetShadowNormalBias() * TexelSize,
                          Light.GetShadowSoftness()};
        Result.Params2 = {1.0f / CAST<f32>(Resolution), FarDistance, FarDistance * 0.15f, Light.GetShadowAmbientDarkening()};
        return Result;
    }

    void MeshRenderer::Render(const Scene& S, const Viewport& Target, const f32 DeltaTime) {
        if (!_Device) return;

        EnsureSceneColorTarget(Target.GetWidth(), Target.GetHeight());

        _Commands.Reset();
        _Commands.PushDebugGroup("Meshes");

        // Cleared every frame, alpha included: alpha is what tells
        // PostProcess's composite pass which pixels of Target to touch at
        // all (see PostProcess.hpp), so an untouched pixel here has to read
        // back as 0, not whatever the last frame's render left behind.
        RHI::RenderPassDesc Pass = RHI::RenderPassDesc::ColorAndDepthTarget(
          _SceneColorTarget, Target.GetDepthTarget(), 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        Pass.DebugName                = "Meshes";
        MeshCache* Meshes             = S.GetContext().Meshes;
        const CameraComponent* Camera = S.GetMainCamera();
        const bool CanDraw            = Meshes && Camera;

        // The frame constants and the shadow pass come first: the shadow map
        // is its own render pass, which has to be recorded before the main
        // pass begins, and the main pass's constants need its light matrix.
        FrameConstants Frame {};
        ShadowState Shadow;
        if (CanDraw) {
            Frame.ViewProjection = Camera->GetViewProjectionMatrix();
            {
                // For Sky.hlsl: unproject a pixel back to a world-space ray.
                using namespace DirectX;
                const XMMATRIX ViewProjection = XMLoadFloat4x4(&Frame.ViewProjection);
                XMStoreFloat4x4(&Frame.InvViewProjection, XMMatrixInverse(nullptr, ViewProjection));
            }

            const Float3 CamPos =
              Camera->GetOwner() ? Camera->GetOwner()->GetWorldTransform().Position : Float3 {0.0f, 0.0f, 0.0f};
            Frame.CameraPositionAndPad = {CamPos.x, CamPos.y, CamPos.z, 0.0f};

            // A default downward light if the scene has none, so a mesh
            // with no light actor still renders as something rather than
            // solid black.
            Float3 LightDir {0.0f, -1.0f, 0.0f};
            Float3 LightColor {1.0f, 1.0f, 1.0f};
            f32 LightIntensity = 1.0f;

            const DirectionalLightComponent* LightComponent = nullptr;
            const std::vector<Actor*> Lights                = S.FindActorsWith<DirectionalLightComponent>();
            if (!Lights.empty()) {
                if (const auto* Light = Lights.front()->GetComponent<DirectionalLightComponent>()) {
                    LightComponent = Light;
                    LightDir       = Light->GetDirection();
                    LightColor     = Light->GetColor();
                    LightIntensity = Light->GetIntensity();
                }
            }
            Frame.LightDirectionAndPad   = {LightDir.x, LightDir.y, LightDir.z, 0.0f};
            Frame.LightColorAndIntensity = {LightColor.x, LightColor.y, LightColor.z, LightIntensity};

            // Only a real light casts shadows - the default downward one
            // above is just a fallback so unlit scenes aren't black.
            if (LightComponent) Shadow = RenderShadowPass(S, *Camera, *LightComponent, LightDir, *Meshes);
            if (Shadow.Enabled) {
                Frame.LightViewProjection = Shadow.LightViewProjection;
                Frame.ShadowParams        = Shadow.Params;
                Frame.ShadowParams2       = Shadow.Params2;
            }
        }

        _Commands.BeginRenderPass(Pass);

        if (CanDraw) {
            _Commands.BindPipeline(_Pipeline);
            _Commands.BindUniformBuffer(MaterialSlot::Frame, _Device->AllocateUniform(Frame));

            // Scene-level IBL, bound once here rather than per actor: none of
            // it varies per draw, and descriptor-table bindings persist
            // across draws until rebound. With no environment (or a bake that
            // failed) the placeholder sky stands in for both maps, so the
            // shader never branches on "is there an environment".
            const EnvironmentState Environment = ResolveEnvironment(S);
            const bool HasBakedEnvironment     = Environment.HasBaked;
            const bool ShowBackground          = Environment.ShowBackground;
            _Commands.BindTexture(MaterialSlot::Environment,
                                  HasBakedEnvironment ? _PrefilteredEnvironment : _DefaultEnvironmentMap,
                                  _EnvironmentSampler);
            _Commands.BindTexture(MaterialSlot::Irradiance,
                                  HasBakedEnvironment ? _IrradianceMap : _DefaultEnvironmentMap,
                                  _EnvironmentSampler);
            _Commands.BindTexture(MaterialSlot::BrdfLut, _BrdfLUT, _ClampSampler);
            _Commands.BindTexture(MaterialSlot::ShadowMap, Shadow.Enabled ? _ShadowMap : _ShadowFallback, _ShadowSampler);

            S.ForEachActor([&](Actor& A) {
                auto* MeshComp     = A.GetComponent<MeshComponent>();
                auto* MaterialComp = A.GetComponent<PBRMaterialComponent>();
                if (!MeshComp || !MaterialComp) return;

                const MeshHandle Mesh = MeshComp->GetMesh();
                if (!Mesh.IsValid()) return;

                const RHI::BufferHandle VertexBuffer = Meshes->GetVertexBuffer(Mesh);
                const RHI::BufferHandle IndexBuffer  = Meshes->GetIndexBuffer(Mesh);
                const MeshInfo Info                  = Meshes->GetInfo(Mesh);
                if (!VertexBuffer.IsValid() || !IndexBuffer.IsValid() || Info.IndexCount == 0) return;

                ObjectConstants Object {};
                Object.Model = A.GetWorldTransform().ToMatrix();

                MaterialConstants Material {};
                const Float3& Albedo       = MaterialComp->GetAlbedo();
                Material.AlbedoAndMetallic = {Albedo.x, Albedo.y, Albedo.z, MaterialComp->GetMetallic()};
                Material.RoughnessAOAndPad = {MaterialComp->GetRoughness(),
                                              MaterialComp->GetAmbientOcclusion(),
                                              0.0f,
                                              0.0f};
                const Float3& Emissive     = MaterialComp->GetEmissive();
                Material.EmissiveAndPad    = {Emissive.x, Emissive.y, Emissive.z, 0.0f};

                _Commands.BindUniformBuffer(MaterialSlot::Object, _Device->AllocateUniform(Object));
                _Commands.BindUniformBuffer(MaterialSlot::Material, _Device->AllocateUniform(Material));

                // Every channel is always bound - a material with no map
                // assigned for a slot falls back to a placeholder that
                // multiplies through as the identity (see PBR.hlsl), so
                // there's no per-material branch to take here either.
                const auto BindChannel = [&](const u32 Slot, const RHI::TextureHandle Handle, const RHI::TextureHandle Fallback) {
                    _Commands.BindTexture(Slot, Handle.IsValid() ? Handle : Fallback, _Sampler);
                };
                BindChannel(MaterialSlot::Albedo, MaterialComp->GetAlbedoMap(), _WhiteTexture);
                BindChannel(MaterialSlot::Normal, MaterialComp->GetNormalMap(), _FlatNormalTexture);
                BindChannel(MaterialSlot::Roughness, MaterialComp->GetRoughnessMap(), _WhiteTexture);
                BindChannel(MaterialSlot::Metallic, MaterialComp->GetMetallicMap(), _WhiteTexture);
                BindChannel(MaterialSlot::AmbientOcclusion, MaterialComp->GetAmbientOcclusionMap(), _WhiteTexture);
                BindChannel(MaterialSlot::Emissive, MaterialComp->GetEmissiveMap(), _WhiteTexture);

                _Commands.BindVertexBuffer(0, VertexBuffer);
                _Commands.BindIndexBuffer(IndexBuffer, Info.IndexType);
                _Commands.DrawIndexed(Info.IndexCount);
            });

            // The background, last: it only lands on pixels no mesh covered
            // (far-plane depth vs LessEqual), and it needs the real baked
            // environment - the placeholder sky isn't something to draw.
            if (HasBakedEnvironment && ShowBackground && _SkyPipeline.IsValid()) {
                _Commands.BindPipeline(_SkyPipeline);
                _Commands.Draw(3);
            }
        }

        _Commands.EndRenderPass();
        _Commands.PopDebugGroup();

        // Settings come from the scene's first PostProcessComponent, the
        // same "first one found" rule as the light and environment; a scene
        // with none renders with PostProcess::Settings's defaults.
        PostProcess::Settings Settings;
        const std::vector<Actor*> PostProcessActors = S.FindActorsWith<PostProcessComponent>();
        if (!PostProcessActors.empty()) {
            if (const auto* PP = PostProcessActors.front()->GetComponent<PostProcessComponent>()) {
                Settings = PP->GetSettings();
            }
        }
        _PostProcess.Render(_Commands,
                           _SceneColorTarget,
                           _SceneColorWidth,
                           _SceneColorHeight,
                           Target.GetColorTarget(),
                           DeltaTime,
                           Settings);

        _Device->Submit(_Commands);
    }
}  // namespace Xen
