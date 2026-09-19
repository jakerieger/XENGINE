//
// Created by Jake Rieger on 9/17/2026.
//
// Turns MeshComponent+PBRMaterialComponent actors into RHI commands - the 3D
// analogue of SpriteRenderer (same "build a CommandBuffer, submit it" shape).

#pragma once

#include <Common/XenCommon.hpp>

#include "RenderDevice.hpp"
#include "Viewport.hpp"

namespace Xen {
    namespace PAK {
        class AssetRegistry;
    }

    class Scene;
    class CameraComponent;

    class MeshRenderer {
    public:
        MeshRenderer() = default;
        ~MeshRenderer();

        MeshRenderer(const MeshRenderer&)            = delete;
        MeshRenderer& operator=(const MeshRenderer&) = delete;

        /// @brief Builds the pipeline from Content/shaders/pbr.hlsl, loaded
        /// through Assets like any other asset - not embedded in C++ the way
        /// SpriteRenderer's shader still is. TargetFormats supplies the
        /// color/depth formats the PSO is built against; it must match
        /// whatever Viewport Render() is later called with.
        bool Initialize(RHI::IRenderDevice& Device, const PAK::AssetRegistry& Assets, const Viewport& TargetFormats);
        void Shutdown();

        NODISCARD bool IsInitialized() const { return _Device != nullptr; }

        /// @brief Records and submits one frame's meshes into Target's color
        /// (and depth) target, lit by the scene's first
        /// DirectionalLightComponent (a flat ambient term stands in for
        /// everything else until IBL exists) and viewed through the scene's
        /// main camera. Call between IRenderDevice::BeginFrame and EndFrame.
        /// Target must have a depth buffer (Viewport::Initialize's
        /// WithDepth) - this renderer always depth-tests.
        void Render(const Scene& S, const Viewport& Target);

    private:
        bool CreateDefaultTextures();

        RHI::IRenderDevice* _Device {nullptr};

        RHI::LayoutHandle _Layout {};
        RHI::PipelineHandle _Pipeline {};
        RHI::CommandBuffer _Commands;

        // Shared by every material texture slot (see MaterialBindings.hpp) -
        // one physical sampler bound repeatedly rather than one per channel,
        // since PBR maps all want the same tiling/filtering behavior.
        RHI::SamplerHandle _Sampler {};

        // Bound for whichever of a material's five channels has no map
        // assigned, so every draw always binds all five textures and the
        // shader never branches on "is this map present" - see PBR.hlsl.
        // White multiplies through as the identity for Albedo/
        // MetallicRoughness/AmbientOcclusion/Emissive; flat-normal
        // (0.5, 0.5, 1.0, decoding to (0,0,1)) is the identity for Normal.
        RHI::TextureHandle _WhiteTexture {};
        RHI::TextureHandle _FlatNormalTexture {};
    };
}  // namespace Xen
