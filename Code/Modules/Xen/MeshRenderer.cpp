//
// Created by Jake Rieger on 9/17/2026.
//

#include "MeshRenderer.hpp"
#include "MaterialBindings.hpp"
#include "MeshComponent.hpp"
#include "PBRMaterialComponent.hpp"
#include "DirectionalLightComponent.hpp"
#include "CameraComponent.hpp"
#include "Actor.hpp"
#include "Scene.hpp"

#include <XenPAK/AssetRegistry.hpp>
#include <XenPAK/AssetBuffer.hpp>

#include <cstring>

namespace Xen {
    namespace {
        struct FrameConstants {
            Float4x4 ViewProjection;
            Float4 CameraPositionAndPad;
            Float4 LightDirectionAndPad;
            Float4 LightColorAndIntensity;
        };

        struct ObjectConstants {
            Float4x4 Model;
        };

        struct MaterialConstants {
            Float4 AlbedoAndMetallic;
            Float4 RoughnessAOAndPad;
            Float4 EmissiveAndPad;
        };
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
        // (model matrix), b2 per-material (constant PBR factors), plus five
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
        PipelineDesc.ColorFormats[0]               = TargetFormats.GetColorFormat();
        PipelineDesc.DepthFormat                   = TargetFormats.GetDepthFormat();
        PipelineDesc.DebugName                     = "XEN.Shaders.PBR";

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
        SamplerDesc.DebugName = "XEN.PBR";
        _Sampler              = Device.CreateSampler(SamplerDesc);

        if (!_Sampler.IsValid() || !CreateDefaultTextures()) {
            Shutdown();
            return false;
        }

        return true;
    }

    bool MeshRenderer::CreateDefaultTextures() {
        // Bound for any material channel with no map assigned (see
        // MeshRenderer.hpp) - both are 1x1 so the cost of always binding
        // them, for a game with no textured materials at all, is trivial.
        const auto MakeSolid = [this](const u8 R, const u8 G, const u8 B, const u8 A, const char* Name) {
            RHI::TextureDesc Desc;
            Desc.Width     = 1;
            Desc.Height    = 1;
            Desc.Fmt       = RHI::Format::RGBA8_UNORM;
            Desc.Usage     = RHI::TextureUsage::Sampled;
            Desc.DebugName = Name;

            const RHI::TextureHandle Handle = _Device->CreateTexture(Desc);
            if (!Handle.IsValid()) return RHI::TextureHandle {};

            const u8 Pixel[4] = {R, G, B, A};
            RHI::TextureUploadDesc Upload;
            Upload.Data     = Pixel;
            Upload.DataSize = sizeof(Pixel);
            Upload.Width    = 1;
            Upload.Height   = 1;
            _Device->UploadTexture(Handle, Upload);

            return Handle;
        };

        _WhiteTexture      = MakeSolid(255, 255, 255, 255, "XEN.PBR.White");
        _FlatNormalTexture = MakeSolid(128, 128, 255, 255, "XEN.PBR.FlatNormal");

        return _WhiteTexture.IsValid() && _FlatNormalTexture.IsValid();
    }

    void MeshRenderer::Shutdown() {
        if (!_Device) return;

        if (_Pipeline.IsValid()) _Device->DestroyPipeline(_Pipeline);
        if (_Layout.IsValid()) _Device->DestroyPipelineLayout(_Layout);
        if (_Sampler.IsValid()) _Device->DestroySampler(_Sampler);
        if (_WhiteTexture.IsValid()) _Device->DestroyTexture(_WhiteTexture);
        if (_FlatNormalTexture.IsValid()) _Device->DestroyTexture(_FlatNormalTexture);

        _Pipeline          = {};
        _Layout            = {};
        _Sampler           = {};
        _WhiteTexture      = {};
        _FlatNormalTexture = {};
        _Device            = nullptr;
    }

    void MeshRenderer::Render(const Scene& S, const Viewport& Target) {
        if (!_Device) return;

        _Commands.Reset();
        _Commands.PushDebugGroup("Meshes");

        RHI::RenderPassDesc Pass = RHI::RenderPassDesc::ColorAndDepthTarget(Target.GetColorTarget(),
                                                                            Target.GetDepthTarget(),
                                                                            0.02f,
                                                                            0.02f,
                                                                            0.03f,
                                                                            1.0f);
        Pass.DebugName           = "Meshes";
        // Color is Load, not this factory's default Clear: Game::TickFrame
        // runs SpriteRenderer::Render() first, unconditionally, every frame
        // regardless of sprite count, and that pass already cleared this
        // same color target - clearing twice would just be redundant, not
        // wrong, but this Load is a real coupling to that ordering, not an
        // independently safe default. If SpriteRenderer's pass is ever
        // skipped when it has nothing to draw, this needs its own Clear
        // again.
        Pass.ColorAttachments[0].Load = RHI::LoadOp::Load;
        _Commands.BeginRenderPass(Pass);

        MeshCache* Meshes             = S.GetContext().Meshes;
        const CameraComponent* Camera = S.GetMainCamera();

        if (Meshes && Camera) {
            FrameConstants Frame {};
            Frame.ViewProjection = Camera->GetViewProjectionMatrix();

            const Float3 CamPos =
              Camera->GetOwner() ? Camera->GetOwner()->GetWorldTransform().Position : Float3 {0.0f, 0.0f, 0.0f};
            Frame.CameraPositionAndPad = {CamPos.x, CamPos.y, CamPos.z, 0.0f};

            // A default downward light if the scene has none, so a mesh
            // with no light actor still renders as something rather than
            // solid black.
            Float3 LightDir {0.0f, -1.0f, 0.0f};
            Float3 LightColor {1.0f, 1.0f, 1.0f};
            f32 LightIntensity = 1.0f;

            const std::vector<Actor*> Lights = S.FindActorsWith<DirectionalLightComponent>();
            if (!Lights.empty()) {
                if (const auto* Light = Lights.front()->GetComponent<DirectionalLightComponent>()) {
                    LightDir       = Light->GetDirection();
                    LightColor     = Light->GetColor();
                    LightIntensity = Light->GetIntensity();
                }
            }
            Frame.LightDirectionAndPad   = {LightDir.x, LightDir.y, LightDir.z, 0.0f};
            Frame.LightColorAndIntensity = {LightColor.x, LightColor.y, LightColor.z, LightIntensity};

            _Commands.BindPipeline(_Pipeline);
            _Commands.BindUniformBuffer(MaterialSlot::Frame, _Device->AllocateUniform(Frame));

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
                BindChannel(MaterialSlot::MetallicRoughness, MaterialComp->GetMetallicRoughnessMap(), _WhiteTexture);
                BindChannel(MaterialSlot::AmbientOcclusion, MaterialComp->GetAmbientOcclusionMap(), _WhiteTexture);
                BindChannel(MaterialSlot::Emissive, MaterialComp->GetEmissiveMap(), _WhiteTexture);

                _Commands.BindVertexBuffer(0, VertexBuffer);
                _Commands.BindIndexBuffer(IndexBuffer, Info.IndexType);
                _Commands.DrawIndexed(Info.IndexCount);
            });
        }

        _Commands.EndRenderPass();
        _Commands.PopDebugGroup();

        _Device->Submit(_Commands);
    }
}  // namespace Xen
