//
// Created by Jake Rieger on 9/17/2026.
//

#include "MeshRenderer.hpp"
#include "MaterialBindings.hpp"
#include "Components/MeshComponent.hpp"
#include "Components/PBRMaterialComponent.hpp"
#include "Components/DirectionalLightComponent.hpp"
#include "Components/EnvironmentComponent.hpp"
#include "Components/PostProcessComponent.hpp"
#include "Components/AmbientOcclusionComponent.hpp"
#include "Components/AntiAliasingComponent.hpp"
#include "Components/CameraComponent.hpp"
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
            Float4x4 ViewProjection;              // JITTERED (see TAA.hpp) - what every pass rasterizes with
            Float4x4 InvViewProjection;            // inverse of the JITTERED matrix - Sky's world-space view ray
            Float4x4 UnjitteredViewProjection;     // this frame, no jitter - TAA's "current" motion-vector clip pos
            Float4x4 InvUnjitteredViewProjection;  // inverse of the above - Sky's motion-vector ray reconstruction
            Float4x4 PrevViewProjection;           // last frame, no jitter - TAA's "previous" motion-vector clip pos
            Float4 CameraPositionAndPad;
            Float4 LightDirectionAndPad;
            Float4 LightColorAndIntensity;
            Float4x4 LightViewProjection;
            Float4 ShadowParams;
            Float4 ShadowParams2;
            Float4 InvScreenSizeAndPad;  // xy = 1 / render target size in pixels - PBR.hlsl's own SSAO screen UV
        };

        struct ObjectConstants {
            Float4x4 Model;
            Float4x4 PrevModel;  // last frame's Model - TAA motion vectors for a moving/rotating actor
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

        // --- Frustum culling ------------------------------------------------
        //
        // A plane as (a, b, c, d): a*x + b*y + c*z + d >= 0 is "inside".
        struct FrustumPlane {
            f32 a, b, c, d;
        };

        // Gribb-Hartmann plane extraction, for DirectXMath's row-vector
        // convention (clip = v * M, so "column c of M" is the linear
        // function of v that produces clip's c-th component - see
        // CameraComponent::GetViewProjectionMatrix's own comment on the
        // convention). D3D's clip volume is -w<=x<=w, -w<=y<=w, 0<=z<=w, so
        // e.g. "Left" (x >= -w) is Column0 + Column3, "Near" (z >= 0) is
        // just Column2 (no D3D-vs-OpenGL 0..1-vs-(-1..1) NDC ambiguity to
        // worry about, unlike the more commonly-copied OpenGL version of
        // this derivation).
        void ExtractFrustumPlanes(const Float4x4& M, FrustumPlane (&Planes)[6]) {
            const auto Col = [&](const int c) -> Float4 {
                return {M.m[0][c], M.m[1][c], M.m[2][c], M.m[3][c]};
            };
            const Float4 C0 = Col(0), C1 = Col(1), C2 = Col(2), C3 = Col(3);

            Planes[0] = {C3.x + C0.x, C3.y + C0.y, C3.z + C0.z, C3.w + C0.w};  // Left
            Planes[1] = {C3.x - C0.x, C3.y - C0.y, C3.z - C0.z, C3.w - C0.w};  // Right
            Planes[2] = {C3.x + C1.x, C3.y + C1.y, C3.z + C1.z, C3.w + C1.w};  // Bottom
            Planes[3] = {C3.x - C1.x, C3.y - C1.y, C3.z - C1.z, C3.w - C1.w};  // Top
            Planes[4] = {C2.x, C2.y, C2.z, C2.w};                             // Near
            Planes[5] = {C3.x - C2.x, C3.y - C2.y, C3.z - C2.z, C3.w - C2.w};  // Far
        }

        // Transforms all 8 corners of a local-space AABB (MeshInfo's
        // BoundsMin/BoundsMax) by Model and takes their min/max - the
        // standard conservative way to get a world-space AABB that fully
        // contains an arbitrarily rotated/scaled/translated box, matching
        // the same 8-corner pattern RenderShadowPass already uses to fit
        // the shadow volume around each caster.
        void ComputeWorldAabb(const Float4x4& Model, const Float3& LocalMin, const Float3& LocalMax,
                              Float3& WorldMin, Float3& WorldMax) {
            using namespace DirectX;
            const XMMATRIX M = XMLoadFloat4x4(&Model);

            WorldMin = {std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max()};
            WorldMax = {std::numeric_limits<f32>::lowest(),
                       std::numeric_limits<f32>::lowest(),
                       std::numeric_limits<f32>::lowest()};

            for (u32 Corner = 0; Corner < 8; ++Corner) {
                const XMVECTOR Local = XMVectorSet((Corner & 1) ? LocalMax.x : LocalMin.x,
                                                   (Corner & 2) ? LocalMax.y : LocalMin.y,
                                                   (Corner & 4) ? LocalMax.z : LocalMin.z,
                                                   1.0f);
                Float3 World;
                XMStoreFloat3(&World, XMVector3Transform(Local, M));

                WorldMin.x = std::min(WorldMin.x, World.x);
                WorldMin.y = std::min(WorldMin.y, World.y);
                WorldMin.z = std::min(WorldMin.z, World.z);
                WorldMax.x = std::max(WorldMax.x, World.x);
                WorldMax.y = std::max(WorldMax.y, World.y);
                WorldMax.z = std::max(WorldMax.z, World.z);
            }
        }

        // Standard "positive vertex" AABB-vs-frustum test: for each plane,
        // the AABB corner furthest along the plane's own normal is the
        // corner most likely to still be inside it - if even that corner
        // fails, the whole box is fully on the outside of that one plane,
        // which is enough to cull it (conservative: never culls something
        // actually visible, may miss culling a box that's outside only at a
        // corner - the standard, cheap trade-off for this technique).
        bool AabbIntersectsFrustum(const Float3& Min, const Float3& Max, const FrustumPlane (&Planes)[6]) {
            for (const FrustumPlane& P : Planes) {
                const f32 PositiveX = P.a >= 0.0f ? Max.x : Min.x;
                const f32 PositiveY = P.b >= 0.0f ? Max.y : Min.y;
                const f32 PositiveZ = P.c >= 0.0f ? Max.z : Min.z;
                if (P.a * PositiveX + P.b * PositiveY + P.c * PositiveZ + P.d < 0.0f) return false;
            }
            return true;
        }
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
        // Engine/XEN.Shaders.pxk, the only place this ever loads them from.
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
        // LessEqual, not the more usual Less: the depth prepass (see
        // RenderDepthPrepass) already wrote every one of these fragments'
        // exact depth before this pass ever runs, so a fragment testing
        // against its own already-written value has to pass, not fail as
        // "not strictly closer than itself" would under Less.
        PipelineDesc.DepthStencil.DepthCompare = RHI::CompareOp::LessEqual;
        // Color plus TAA motion vectors (MRT - see PBR.hlsl's PSOutput,
        // TAA.hpp): both written by the same draw, same render pass.
        PipelineDesc.ColorAttachmentCount          = 2;
        PipelineDesc.ColorFormats[0]               = SceneColorFormat;
        PipelineDesc.ColorFormats[1]               = RHI::Format::RG16_FLOAT;
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
                    SkyDesc.ColorAttachmentCount            = 2;  // color + motion vectors (MRT) - same as _Pipeline
                    SkyDesc.ColorFormats[0]                 = SceneColorFormat;
                    SkyDesc.ColorFormats[1]                 = RHI::Format::RG16_FLOAT;
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

        CreateShadowPipeline(Assets, TargetFormats.GetDepthFormat());

        // Not fatal: without it a scene just renders with no ambient
        // occlusion (Render binds _WhiteTexture into the SSAO slot instead
        // - see MeshRenderer.hpp).
        _SSAO.Initialize(Device, Assets);

        // Not fatal: without it a scene with TAA picked just falls back to
        // an unresolved (and un-jittered - see Render's UseTaa check)
        // scene color, same as if AntiAliasingComponent::Technique were
        // None.
        _TAA.Initialize(Device, Assets, SceneColorFormat);

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

    void MeshRenderer::CreateShadowPipeline(const PAK::AssetRegistry& Assets, const RHI::Format TargetDepthFormat) {
        constexpr AssetID VertexAsset = ASSET("xen.shader.shadow.vs");
        if (!Assets.Contains(VertexAsset)) {
            LOG_WARN("shadow shader not found - directional lights will not cast shadows or have SSAO");
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
        // slot) and the model matrix (b1): no textures, no material. Shared
        // by both pipelines below - the shader (and so this layout) has no
        // idea whether it's about to be used for the light's shadow map or
        // the camera's own depth prepass.
        RHI::PipelineLayoutDesc LayoutDesc;
        LayoutDesc.Binding(MaterialSlot::Frame, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::All)
          .Binding(MaterialSlot::Object, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::All);
        LayoutDesc.DebugName = "XEN.Shaders.Shadow";
        _ShadowLayout        = _Device->CreatePipelineLayout(LayoutDesc);

        RHI::VertexLayout VertexLayout;
        VertexLayout.Binding(0, sizeof(MeshVertex), RHI::VertexInputRate::Vertex)
          .Attribute(0, 0, RHI::Format::RGB32_FLOAT, offsetof(MeshVertex, Position));

        if (Vertex.IsValid() && _ShadowLayout.IsValid()) {
            RHI::GraphicsPipelineDesc PipelineDesc;
            PipelineDesc.VertexShader   = Vertex;  // no fragment shader: depth only
            PipelineDesc.PipelineLayout = _ShadowLayout;
            PipelineDesc.Topology       = RHI::PrimitiveTopology::TriangleList;
            PipelineDesc.Layout         = VertexLayout;

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

        // The depth prepass: same shader and vertex layout, but a real
        // camera view - ordinary back-face culling applies, and the target
        // is whatever format Target's own depth buffer actually is.
        if (Vertex.IsValid() && _ShadowLayout.IsValid()) {
            RHI::GraphicsPipelineDesc PipelineDesc;
            PipelineDesc.VertexShader                  = Vertex;
            PipelineDesc.PipelineLayout                = _ShadowLayout;
            PipelineDesc.Topology                      = RHI::PrimitiveTopology::TriangleList;
            PipelineDesc.Layout                        = VertexLayout;
            PipelineDesc.Rasterizer.Cull                = RHI::CullMode::Back;
            PipelineDesc.DepthStencil.DepthTestEnable  = true;
            PipelineDesc.DepthStencil.DepthWriteEnable = true;
            PipelineDesc.DepthStencil.DepthCompare     = RHI::CompareOp::Less;
            PipelineDesc.ColorAttachmentCount          = 0;
            PipelineDesc.DepthFormat                   = TargetDepthFormat;
            PipelineDesc.DebugName                     = "XEN.Shaders.DepthPrepass";
            _DepthPrepassPipeline                      = _Device->CreateGraphicsPipeline(PipelineDesc);
        }

        if (Vertex.IsValid()) _Device->DestroyShader(Vertex);

        if (!_DepthPrepassPipeline.IsValid()) {
            LOG_WARN("failed to create the depth prepass pipeline - SSAO will be unavailable");
        }
        if (!_ShadowPipeline.IsValid()) {
            LOG_WARN("failed to create the shadow pipeline - directional lights will not cast shadows");
        }

        // _ShadowLayout is shared by both pipelines above - only tear it
        // down once neither one is left referencing it, or whichever one DID
        // build would be left pointing at a destroyed layout.
        if (!_ShadowPipeline.IsValid() && !_DepthPrepassPipeline.IsValid() && _ShadowLayout.IsValid()) {
            _Device->DestroyPipelineLayout(_ShadowLayout);
            _ShadowLayout = {};
        }
    }

    void MeshRenderer::Shutdown() {
        if (!_Device) return;

        ReleaseBakedEnvironment();
        _Baker.Shutdown();
        _PostProcess.Shutdown();
        _SSAO.Shutdown();
        _TAA.Shutdown();
        if (_SceneColorTarget.IsValid()) _Device->DestroyTexture(_SceneColorTarget);
        if (_MotionVectorsTarget.IsValid()) _Device->DestroyTexture(_MotionVectorsTarget);
        _PrevModelMatrices.clear();
        _HasPrevViewProjection = false;

        if (_ShadowPipeline.IsValid()) _Device->DestroyPipeline(_ShadowPipeline);
        if (_DepthPrepassPipeline.IsValid()) _Device->DestroyPipeline(_DepthPrepassPipeline);
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
        _DepthPrepassPipeline  = {};
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
        _MotionVectorsTarget   = {};
        _SceneColorWidth       = 0;
        _SceneColorHeight      = 0;
        _Device                = nullptr;
    }

    void MeshRenderer::EnsureSceneColorTarget(const u32 Width, const u32 Height) {
        if (_SceneColorTarget.IsValid() && _SceneColorWidth == Width && _SceneColorHeight == Height) return;

        if (_SceneColorTarget.IsValid()) _Device->DestroyTexture(_SceneColorTarget);
        if (_MotionVectorsTarget.IsValid()) _Device->DestroyTexture(_MotionVectorsTarget);

        RHI::TextureDesc Desc;
        Desc.Width     = Width;
        Desc.Height    = Height;
        Desc.Fmt       = SceneColorFormat;
        Desc.Usage     = RHI::TextureUsage::ColorTarget | RHI::TextureUsage::Sampled;
        Desc.DebugName = "XEN.PBR.SceneColor";

        _SceneColorTarget = _Device->CreateTexture(Desc);

        // TAA motion vectors (see TAA.hpp, PBR.hlsl/Sky.hlsl's PSOutput) -
        // the main pass's second render target (MRT), same size as color.
        // RG16F: a screen-space UV displacement is a tiny 2-component value,
        // no need for RGBA32F precision.
        Desc.Fmt              = RHI::Format::RG16_FLOAT;
        Desc.DebugName        = "XEN.PBR.MotionVectors";
        _MotionVectorsTarget  = _Device->CreateTexture(Desc);

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

    void MeshRenderer::RenderDepthPrepass(const std::vector<VisibleMesh>& Visible, const Float4x4& ViewProjection) {
        if (!_DepthPrepassPipeline.IsValid()) return;

        // BindPipeline first: it's what may change the root signature (a
        // different pipeline layout than whatever was bound last - see
        // CmdType::BindPipeline), which invalidates every previously-bound
        // root argument. Binding Frame before it, the way this read until
        // now, recorded that bind against the wrong (stale) root signature -
        // exactly what RenderShadowPass already gets right, which is what
        // this should have matched from the start.
        _Commands.BindPipeline(_DepthPrepassPipeline);

        FrameConstants Frame {};
        Frame.ViewProjection = ViewProjection;
        _Commands.BindUniformBuffer(MaterialSlot::Frame, _Device->AllocateUniform(Frame));

        // Visible is already frustum-culled and mesh/buffer-validated (see
        // MeshRenderer::Render) - nothing left to skip here.
        for (const VisibleMesh& VM : Visible) {
            ObjectConstants Object {};
            Object.Model = VM.Model;
            _Commands.BindUniformBuffer(MaterialSlot::Object, _Device->AllocateUniform(Object));
            _Commands.BindVertexBuffer(0, VM.VertexBuffer);
            _Commands.BindIndexBuffer(VM.IndexBuffer, VM.Info.IndexType);
            _Commands.DrawIndexed(VM.Info.IndexCount);
        }
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
        Pass.DebugName = "Meshes";

        // TAA motion vectors (see PBR.hlsl/Sky.hlsl's PSOutput), written
        // alongside color as this pass's second render target (MRT) - see
        // EnsureSceneColorTarget. Cleared to zero velocity every frame, same
        // reasoning as color's own alpha clear: a pixel nothing draws into
        // should read back as "didn't move", not whatever the last frame's
        // render left behind.
        Pass.ColorAttachmentCount          = 2;
        Pass.ColorAttachments[1].Texture   = _MotionVectorsTarget;
        Pass.ColorAttachments[1].Load      = RHI::LoadOp::Clear;
        Pass.ColorAttachments[1].Clear     = RHI::ClearValue {{0.0f, 0.0f, 0.0f, 0.0f}, 1.0f, 0};

        MeshCache* Meshes             = S.GetContext().Meshes;
        const CameraComponent* Camera = S.GetMainCamera();
        const bool CanDraw            = Meshes && Camera;

        // Settings come from the scene's first AntiAliasingComponent, the
        // same "first one found" rule as PostProcess/AO; a scene with none
        // uses Technique's own default (TAA). Resolved before Frame is
        // built: whether to jitter the projection at all has to be decided
        // before ViewProjection is.
        AntiAliasingTechnique AaTechnique = AntiAliasingTechnique::TAA;
        TAA::Settings TaaSettings;
        const std::vector<Actor*> AaActors = S.FindActorsWith<AntiAliasingComponent>();
        if (!AaActors.empty()) {
            if (const auto* AA = AaActors.front()->GetComponent<AntiAliasingComponent>()) {
                AaTechnique = AA->GetTechnique();
                TaaSettings = AA->GetTaaSettings();
            }
        }
        const bool UseTaa = AaTechnique == AntiAliasingTechnique::TAA && TaaSettings.Enabled && _TAA.IsInitialized();

        // The frame constants and the shadow pass come first: the shadow map
        // is its own render pass, which has to be recorded before the main
        // pass begins, and the main pass's constants need its light matrix.
        FrameConstants Frame {};
        ShadowState Shadow;
        if (CanDraw) {
            using namespace DirectX;

            const Float4x4 ViewF = Camera->GetViewMatrix();
            const Float4x4 ProjF = Camera->GetProjectionMatrix();
            const XMMATRIX View  = XMLoadFloat4x4(&ViewF);
            const XMMATRIX UnjitteredProj = XMLoadFloat4x4(&ProjF);

            // TAA jitter: a sub-pixel offset added to the projection
            // matrix's translation-of-x/y-by-z row (see TAA.hpp) - only
            // when TAA will actually resolve it away; an unresolved
            // jittered frame would just wobble. Applied to Proj alone,
            // before composing with View: injecting it into the already-
            // composed ViewProjection instead would scale it by world-space
            // z rather than view-space z, which is wrong for any rotated
            // camera (see TAA.hpp's own design notes on this).
            XMMATRIX JitteredProj = UnjitteredProj;
            if (UseTaa) {
                const Float2 Jitter                       = _TAA.GetJitterOffset(Target.GetWidth(), Target.GetHeight());
                XMFLOAT4X4 JitteredProjF;
                XMStoreFloat4x4(&JitteredProjF, UnjitteredProj);
                JitteredProjF.m[2][0] += Jitter.x;
                JitteredProjF.m[2][1] += Jitter.y;
                JitteredProj = XMLoadFloat4x4(&JitteredProjF);
            }

            const XMMATRIX JitteredVP   = View * JitteredProj;
            const XMMATRIX UnjitteredVP = View * UnjitteredProj;

            XMStoreFloat4x4(&Frame.ViewProjection, JitteredVP);
            XMStoreFloat4x4(&Frame.UnjitteredViewProjection, UnjitteredVP);
            // For Sky.hlsl: unproject a pixel back to a world-space ray
            // (JITTERED - matches every other pass this frame rasterizes
            // with) and, separately, its own UNJITTERED motion-vector
            // reconstruction (see PSOutput in Sky.hlsl).
            XMStoreFloat4x4(&Frame.InvViewProjection, XMMatrixInverse(nullptr, JitteredVP));
            XMStoreFloat4x4(&Frame.InvUnjitteredViewProjection, XMMatrixInverse(nullptr, UnjitteredVP));

            Frame.PrevViewProjection = _HasPrevViewProjection ? _PrevViewProjection : Frame.UnjitteredViewProjection;
            _PrevViewProjection      = Frame.UnjitteredViewProjection;
            _HasPrevViewProjection   = true;

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

            Frame.InvScreenSizeAndPad = {1.0f / CAST<f32>(Target.GetWidth()), 1.0f / CAST<f32>(Target.GetHeight()), 0.0f, 0.0f};
        }

        // Frustum culling: every mesh actor, tested once against the
        // camera's view frustum (a plain AABB-vs-6-planes test - see
        // ExtractFrustumPlanes/AabbIntersectsFrustum) and, if visible,
        // resolved down to everything both the depth prepass and the main
        // pass need to draw it - so neither pass repeats the mesh/buffer
        // lookup or the world-matrix computation, and neither iterates an
        // actor this frame will never actually rasterize. Jitter (see
        // TAA.hpp) is sub-pixel, so testing against the jittered
        // Frame.ViewProjection instead of an unjittered variant makes no
        // practical difference here.
        std::vector<VisibleMesh> VisibleMeshes;
        u32 CulledMeshCount = 0;
        if (CanDraw) {
            FrustumPlane Planes[6];
            ExtractFrustumPlanes(Frame.ViewProjection, Planes);

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

                const Float4x4 Model = A.GetWorldTransform().ToMatrix();

                Float3 WorldMin, WorldMax;
                ComputeWorldAabb(Model, Info.BoundsMin, Info.BoundsMax, WorldMin, WorldMax);
                if (!AabbIntersectsFrustum(WorldMin, WorldMax, Planes)) {
                    ++CulledMeshCount;
                    return;
                }

                VisibleMeshes.push_back({&A, MaterialComp, Model, VertexBuffer, IndexBuffer, Info});
            });
        }
        _LastCulledMeshCount  = CulledMeshCount;
        _LastVisibleMeshCount = CAST<u32>(VisibleMeshes.size());

        // Camera-space depth prepass + SSAO, both before the main pass
        // begins - see MeshRenderer.hpp's own comment on RenderDepthPrepass
        // for why a forward renderer needs the depth done this early at all.
        // Neither touches _SceneColorTarget; both use Target's own depth
        // buffer, which the main pass below then reads back with Load
        // instead of clearing it.
        RHI::TextureHandle AoTexture;
        if (CanDraw) {
            {
                RHI::RenderPassDesc PrepassDesc;
                PrepassDesc.HasDepthStencil          = true;
                PrepassDesc.DepthStencil.Texture     = Target.GetDepthTarget();
                PrepassDesc.DepthStencil.DepthLoad   = RHI::LoadOp::Clear;
                PrepassDesc.DepthStencil.DepthStore  = RHI::StoreOp::Store;
                PrepassDesc.DepthStencil.Clear.Depth = 1.0f;
                PrepassDesc.DebugName                = "Depth prepass";
                _Commands.PushDebugGroup("Depth prepass");
                _Commands.BeginRenderPass(PrepassDesc);
                RenderDepthPrepass(VisibleMeshes, Frame.ViewProjection);
                _Commands.EndRenderPass();
                _Commands.PopDebugGroup();
            }

            // Settings come from the scene's first AmbientOcclusionComponent,
            // the same "first one found" rule as PostProcessComponent; a
            // scene with none uses SSAO::Settings's defaults (on).
            SSAO::Settings AoSettings;
            const std::vector<Actor*> AoActors = S.FindActorsWith<AmbientOcclusionComponent>();
            if (!AoActors.empty()) {
                if (const auto* Ao = AoActors.front()->GetComponent<AmbientOcclusionComponent>()) {
                    AoSettings = Ao->GetSettings();
                }
            }
            const Float3 CameraPosition = {
              Frame.CameraPositionAndPad.x, Frame.CameraPositionAndPad.y, Frame.CameraPositionAndPad.z};
            AoTexture = _SSAO.Render(_Commands,
                                     Target.GetDepthTarget(),
                                     Frame.ViewProjection,
                                     Frame.InvViewProjection,
                                     CameraPosition,
                                     Target.GetWidth(),
                                     Target.GetHeight(),
                                     AoSettings);
        }

        // LoadOp::Load, not the default Clear this helper would otherwise
        // pick: the depth prepass above already fully populated Target's
        // depth buffer, and the main pipeline's DepthCompare is LessEqual
        // specifically so re-testing every fragment against that already-
        // written value here passes instead of failing (see Initialize).
        // Only when the prepass actually ran (CanDraw) - otherwise nothing
        // cleared Target's depth buffer this frame at all, and Load would
        // read back whatever a previous frame (or nothing, on the very
        // first one) left there.
        if (CanDraw) Pass.DepthStencil.DepthLoad = RHI::LoadOp::Load;

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
            // White (fully unoccluded) when SSAO is off/unavailable -
            // multiplies through as the identity, the same convention as
            // every material channel's own placeholder.
            _Commands.BindTexture(MaterialSlot::SSAO, AoTexture.IsValid() ? AoTexture : _WhiteTexture, _ClampSampler);

            for (const VisibleMesh& VM : VisibleMeshes) {
                Actor& A                        = *VM.A;
                PBRMaterialComponent* MaterialComp = VM.Material;

                ObjectConstants Object {};
                Object.Model = VM.Model;

                // TAA motion vectors (see PBR.hlsl's PSOutput): last frame's
                // Model, keyed by this actor's stable handle - a new actor
                // (no entry yet) falls back to its own current Model, i.e.
                // "assumed stationary" for exactly one frame.
                const ActorHandle Handle = A.GetHandle();
                const auto PrevModelIt   = _PrevModelMatrices.find(Handle);
                Object.PrevModel = PrevModelIt != _PrevModelMatrices.end() ? PrevModelIt->second : Object.Model;
                _PrevModelMatrices[Handle] = Object.Model;

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

                _Commands.BindVertexBuffer(0, VM.VertexBuffer);
                _Commands.BindIndexBuffer(VM.IndexBuffer, VM.Info.IndexType);
                _Commands.DrawIndexed(VM.Info.IndexCount);
            }

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

        // TAA resolve, before PostProcess: it needs the linear-HDR scene
        // color and motion vectors just written above, and its own output
        // (temporally antialiased, still linear HDR) is what PostProcess
        // should tonemap instead - see TAA.hpp. A no-op (returns
        // _SceneColorTarget unchanged) when TAA isn't this scene's chosen
        // technique or its shader never loaded.
        const RHI::TextureHandle ResolvedColor =
          CanDraw && UseTaa
            ? _TAA.Resolve(
                _Commands, _SceneColorTarget, _MotionVectorsTarget, _SceneColorWidth, _SceneColorHeight, TaaSettings)
            : _SceneColorTarget;

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
                           ResolvedColor,
                           _SceneColorWidth,
                           _SceneColorHeight,
                           Target.GetColorTarget(),
                           DeltaTime,
                           Settings);

        _Device->Submit(_Commands);
    }
}  // namespace Xen
