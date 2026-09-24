//
// Created by Jake Rieger on 9/24/2026.
//

#include "LightCulling.hpp"
#include "MaterialBindings.hpp"

#include <XenPAK/AssetRegistry.hpp>
#include <XenPAK/AssetBuffer.hpp>

#include <vector>

namespace Xen {
    namespace {
        // Matches Code/Shaders/LightCulling.hlsl's own local cbuffer exactly -
        // not FrameData.hlsli, same "own small Params cbuffer" convention
        // SSAO.hlsl uses rather than sharing MeshRenderer's much larger one.
        struct LightCullParams {
            Float4x4 InvViewProjection;
            Float4 ScreenAndTileDim;  // x = screen width, y = screen height (pixels), z = tile count X, w = tile count Y
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

    LightCulling::~LightCulling() {
        Shutdown();
    }

    bool LightCulling::Initialize(RHI::IRenderDevice& Device, const PAK::AssetRegistry& Assets) {
        _Device = &Device;

        RHI::SamplerDesc SamplerDesc;
        SamplerDesc.MinFilter = RHI::FilterMode::Nearest;
        SamplerDesc.MagFilter = RHI::FilterMode::Nearest;
        SamplerDesc.MipFilter = RHI::MipMode::None;
        SamplerDesc.AddressU  = RHI::AddressMode::ClampToEdge;
        SamplerDesc.AddressV  = RHI::AddressMode::ClampToEdge;
        SamplerDesc.DebugName = "XEN.Shaders.LightCulling.Sampler";
        _Sampler              = Device.CreateSampler(SamplerDesc);

        const RHI::ShaderHandle Compute = LoadShader(
          Device, Assets, ASSET("xen.shader.lightculling.cs"), RHI::ShaderStage::Compute, "XEN.Shaders.LightCulling");

        RHI::PipelineLayoutDesc LayoutDesc;
        LayoutDesc.Binding(0, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::Compute)     // LightCullParams
          .Binding(MaterialSlot::LightData, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::Compute)
          .Binding(0, RHI::BindingType::SampledTexture, RHI::ShaderVisibility::Compute)  // Depth
          .Binding(0, RHI::BindingType::Sampler, RHI::ShaderVisibility::Compute)
          .Binding(MaterialSlot::LightIndexList, RHI::BindingType::StorageBuffer, RHI::ShaderVisibility::Compute)
          .Binding(MaterialSlot::TileLightGrid, RHI::BindingType::StorageBuffer, RHI::ShaderVisibility::Compute);
        LayoutDesc.DebugName = "XEN.Shaders.LightCulling";
        _Layout               = Device.CreatePipelineLayout(LayoutDesc);

        if (Compute.IsValid() && _Layout.IsValid()) {
            RHI::ComputePipelineDesc Desc;
            Desc.ComputeShader = Compute;
            Desc.PipelineLayout = _Layout;
            Desc.DebugName      = "XEN.Shaders.LightCulling";
            _Pipeline           = Device.CreateComputePipeline(Desc);
        }

        if (Compute.IsValid()) Device.DestroyShader(Compute);

        return true;  // _Device is set regardless - see IsInitialized's own comment
    }

    void LightCulling::Shutdown() {
        if (!_Device) return;

        if (_Pipeline.IsValid()) _Device->DestroyPipeline(_Pipeline);
        if (_Layout.IsValid()) _Device->DestroyPipelineLayout(_Layout);
        if (_Sampler.IsValid()) _Device->DestroySampler(_Sampler);
        if (_LightIndexListBuffer.IsValid()) _Device->DestroyBuffer(_LightIndexListBuffer);
        if (_TileLightGridBuffer.IsValid()) _Device->DestroyBuffer(_TileLightGridBuffer);

        _Pipeline              = {};
        _Layout                = {};
        _Sampler                = {};
        _LightIndexListBuffer  = {};
        _TileLightGridBuffer   = {};
        _TileCountX = _TileCountY = 0;
        _Device                = nullptr;
    }

    void LightCulling::EnsureBuffers(const u32 TileCountX, const u32 TileCountY) {
        if (TileCountX == _TileCountX && TileCountY == _TileCountY && _LightIndexListBuffer.IsValid()) return;

        if (_LightIndexListBuffer.IsValid()) _Device->DestroyBuffer(_LightIndexListBuffer);
        if (_TileLightGridBuffer.IsValid()) _Device->DestroyBuffer(_TileLightGridBuffer);

        const u32 TileCount = TileCountX * TileCountY;

        RHI::BufferDesc IndexDesc;
        IndexDesc.Size      = CAST<u64>(TileCount) * MaxLightsPerTile * sizeof(u32);
        IndexDesc.Usage     = RHI::BufferUsage::Storage;
        IndexDesc.Memory    = RHI::MemoryUsage::GpuOnly;
        IndexDesc.DebugName = "XEN.LightCulling.LightIndexList";
        _LightIndexListBuffer = _Device->CreateBuffer(IndexDesc);

        // Zero-filled at creation so a tile's count reads 0 (no lights) until
        // a real Dispatch overwrites it - the fallback this class promises
        // when its compute shader asset never loaded (see Render).
        const std::vector<u32> ZeroGrid(TileCount, 0);
        RHI::BufferDesc GridDesc;
        GridDesc.Size        = CAST<u64>(TileCount) * sizeof(u32);
        GridDesc.Usage       = RHI::BufferUsage::Storage;
        GridDesc.Memory      = RHI::MemoryUsage::GpuOnly;
        GridDesc.InitialData = ZeroGrid.data();
        GridDesc.DebugName   = "XEN.LightCulling.TileLightGrid";
        _TileLightGridBuffer = _Device->CreateBuffer(GridDesc);

        _TileCountX = TileCountX;
        _TileCountY = TileCountY;
    }

    LightCulling::Result LightCulling::Render(RHI::CommandBuffer& Commands,
                                              const RHI::TextureHandle Depth,
                                              const Float4x4& InvViewProjection,
                                              const LightConstants& Lights,
                                              const u32 Width,
                                              const u32 Height) {
        Result Out;
        if (!_Device || Width == 0 || Height == 0) return Out;

        const u32 TileCountX = (Width + TileSize - 1) / TileSize;
        const u32 TileCountY = (Height + TileSize - 1) / TileSize;
        EnsureBuffers(TileCountX, TileCountY);

        Out.LightIndexList = _LightIndexListBuffer;
        Out.TileLightGrid  = _TileLightGridBuffer;
        Out.TileCountX     = TileCountX;
        Out.TileCountY     = TileCountY;

        // Shader asset unavailable (e.g. not yet recompiled) - every tile
        // stays at the zero-filled count EnsureBuffers just gave it, same
        // "not fatal, just no effect" convention as every other optional
        // MeshRenderer subsystem.
        if (!_Pipeline.IsValid()) return Out;

        LightCullParams Params {};
        Params.InvViewProjection = InvViewProjection;
        Params.ScreenAndTileDim  = {CAST<f32>(Width), CAST<f32>(Height), CAST<f32>(TileCountX), CAST<f32>(TileCountY)};

        Commands.BindPipeline(_Pipeline);
        Commands.BindUniformBuffer(0, _Device->AllocateUniform(Params));
        Commands.BindUniformBuffer(MaterialSlot::LightData, _Device->AllocateUniform(Lights));
        Commands.BindTexture(0, Depth, _Sampler);
        Commands.BindStorageBuffer(MaterialSlot::LightIndexList, _LightIndexListBuffer);
        Commands.BindStorageBuffer(MaterialSlot::TileLightGrid, _TileLightGridBuffer);
        Commands.Dispatch(TileCountX, TileCountY, 1);

        return Out;
    }
}  // namespace Xen
