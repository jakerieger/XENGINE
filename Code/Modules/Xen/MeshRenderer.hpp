//
// Created by Jake Rieger on 9/17/2026.
//
// Turns MeshComponent+PBRMaterialComponent actors into RHI commands - the 3D
// analogue of SpriteRenderer (same "build a CommandBuffer, submit it" shape).

#pragma once

#include <Common/XenCommon.hpp>

#include "EnvironmentBaker.hpp"
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
        /// DirectionalLightComponent plus image-based lighting from its first
        /// EnvironmentComponent (a dim placeholder sky if it has none), and
        /// viewed through the scene's main camera. Call between
        /// IRenderDevice::BeginFrame and EndFrame.
        /// Target must have a depth buffer (Viewport::Initialize's
        /// WithDepth) - this renderer always depth-tests.
        void Render(const Scene& S, const Viewport& Target);

        /// @brief Bakes the scene's environment (the prefiltered/irradiance
        /// cube maps) now instead of on the first Render that sees it. The
        /// bake is a burst of GPU work, so a game calls this while its loading
        /// screen is still up rather than hitching the first real frame. Must
        /// be called between BeginFrame and EndFrame, like Render; a no-op if
        /// the scene has no environment or it's already baked.
        void PrepareEnvironment(const Scene& S);

    private:
        bool CreateDefaultTextures();

        /// @brief Renders the split-sum BRDF lookup table (see
        /// Code/Shaders/BRDFIntegrate.hlsl) into _BrdfLUT once, synchronously
        /// - it depends on nothing but the pixel's own position, so there's
        /// no reason to ever redo it.
        bool BakeBrdfLut(const PAK::AssetRegistry& Assets);

        RHI::IRenderDevice* _Device {nullptr};

        RHI::LayoutHandle _Layout {};
        RHI::PipelineHandle _Pipeline {};
        RHI::PipelineHandle _SkyPipeline {};  // optional - see Initialize
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

        // Scene-level IBL resources (see MaterialBindings.hpp's Environment/
        // BrdfLut slots) - bound once per frame, not per draw.
        //
        // _EnvironmentSampler wraps in U (longitude is a circle) but clamps in
        // V: an equirectangular map's top/bottom rows are the poles, and
        // wrapping V would bilinear-blend the zenith into the nadir.
        // _ClampSampler is for the LUT, a finite [0,1]^2 table, not tiled.
        RHI::SamplerHandle _EnvironmentSampler {};
        RHI::SamplerHandle _ClampSampler {};

        // Bound whenever the scene has no EnvironmentComponent (or one with
        // no map): a tiny sky/ground gradient cube, so the shader always
        // samples a real texture and never branches on "is there an
        // environment".
        RHI::TextureHandle _DefaultEnvironmentMap {};
        RHI::TextureHandle _BrdfLUT {};

        // The scene's environment, baked lazily the first frame Render() sees
        // one (see ResolveEnvironment): _BakedSource is the raw TextureCache
        // handle the two baked maps were made from, so a different (or
        // removed) environment is noticed by comparing against it.
        EnvironmentBaker _Baker;
        RHI::TextureHandle _BakedSource {};
        RHI::TextureHandle _PrefilteredEnvironment {};
        RHI::TextureHandle _IrradianceMap {};

        void ReleaseBakedEnvironment();

        struct EnvironmentState {
            bool HasBaked {false};
            bool ShowBackground {false};
        };

        /// @brief Finds the scene's environment, (re)bakes it if it changed,
        /// and reports what's now bound-able. Records into the current frame
        /// (the bake uses Submit), so only call it between BeginFrame and
        /// EndFrame.
        EnvironmentState ResolveEnvironment(const Scene& S);
    };
}  // namespace Xen
