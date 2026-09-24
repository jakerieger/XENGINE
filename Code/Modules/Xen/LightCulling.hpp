//
// Created by Jake Rieger on 9/24/2026.
//
// Forward+ tile light culling: a compute pass, run once per frame right
// after MeshRenderer's depth prepass, that sorts every point/spot light in
// LightData.hlsli's flat array into a per-screen-tile index list. PBR.hlsl's
// pixel shader then loops only over the handful of lights that actually
// overlap its own tile instead of every light in the scene - the difference
// between this and the flat forward loop it replaces (see LightData.hpp).
//
// Mirrors SSAO.hpp's shape: owns its own pipeline/layout/sampler, stores
// _Device from Initialize, and Render() records into the caller's own
// CommandBuffer and hands back what it produced rather than submitting
// anything itself.

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Math.hpp>

#include "LightData.hpp"
#include "RenderDevice.hpp"

namespace Xen {
    namespace PAK {
        class AssetRegistry;
    }

    class LightCulling {
    public:
        // Tile size in pixels and the per-tile capacity of the index list -
        // kept in sync by hand with Code/Shaders/Include/LightCulling.hlsli
        // (XEN_TILE_SIZE/XEN_MAX_LIGHTS_PER_TILE), same convention as
        // LightData.hpp's MaxLights. Public: MeshRenderer needs TileSize too,
        // to compute FrameData's TileGridAndSize independently of this
        // class's own Render() call (see MeshRenderer.cpp).
        static constexpr u32 TileSize        = 16;
        static constexpr u32 MaxLightsPerTile = 64;

        LightCulling() = default;
        ~LightCulling();

        LightCulling(const LightCulling&)            = delete;
        LightCulling& operator=(const LightCulling&) = delete;

        bool Initialize(RHI::IRenderDevice& Device, const PAK::AssetRegistry& Assets);
        void Shutdown();

        NODISCARD bool IsInitialized() const { return _Device != nullptr; }

        struct Result {
            RHI::BufferHandle LightIndexList {};
            RHI::BufferHandle TileLightGrid {};
            u32 TileCountX {0};
            u32 TileCountY {0};

            NODISCARD bool IsValid() const { return LightIndexList.IsValid() && TileLightGrid.IsValid(); }
        };

        /// @brief Dispatches the culling pass into Commands (needs Depth
        /// already fully rendered - MeshRenderer's own depth prepass, same
        /// dependency SSAO::Render has) and returns the two persistent
        /// buffers PBR.hlsl reads per-pixel. Always returns valid handles
        /// once IsInitialized() (see EnsureBuffers) even if the compute
        /// shader asset itself failed to load - in that case TileLightGrid
        /// is left zero-filled from creation (every tile reports 0 lights,
        /// same "not fatal" convention as every other optional MeshRenderer
        /// subsystem), and no Dispatch is recorded. The caller still owns
        /// the UAV->read PipelineBarrier before binding the result for
        /// reading (only it knows when the main pass will actually do
        /// that) - see MeshRenderer::Render.
        Result Render(RHI::CommandBuffer& Commands,
                      RHI::TextureHandle Depth,
                      const Float4x4& InvViewProjection,
                      const LightConstants& Lights,
                      u32 Width,
                      u32 Height);

    private:
        void EnsureBuffers(u32 TileCountX, u32 TileCountY);

        RHI::IRenderDevice* _Device {nullptr};

        RHI::LayoutHandle _Layout {};
        RHI::PipelineHandle _Pipeline {};  // LightCulling.hlsl - optional, see Render
        RHI::SamplerHandle _Sampler {};    // point, clamped - depth is Load()'d, not filtered

        RHI::BufferHandle _LightIndexListBuffer {};
        RHI::BufferHandle _TileLightGridBuffer {};
        u32 _TileCountX {0};
        u32 _TileCountY {0};
    };
}  // namespace Xen
