//
// Created by Jake Rieger on 9/17/2026.
//
// Turns MeshComponent+PBRMaterialComponent actors into RHI commands - the 3D
// analogue of SpriteRenderer (same "build a CommandBuffer, submit it" shape).

#pragma once

#include <Common/Math.hpp>
#include <Common/XenCommon.hpp>

#include "EnvironmentBaker.hpp"
#include "PostProcess.hpp"
#include "RenderDevice.hpp"
#include "Viewport.hpp"

namespace Xen {
    namespace PAK {
        class AssetRegistry;
    }

    class Scene;
    class CameraComponent;
    class DirectionalLightComponent;
    class MeshCache;

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

        /// @brief Records and submits one frame's meshes, lit by the scene's
        /// first DirectionalLightComponent (which casts shadows onto them -
        /// one shadow map rendered first, fitted to the camera's view out to
        /// the light's ShadowDistance) plus image-based lighting from its
        /// first EnvironmentComponent (a dim placeholder sky if it has
        /// none), viewed through the scene's main camera. Rendered into a
        /// private linear-HDR target first, not Target's color target
        /// directly - PostProcess's exposure/bloom/tonemap composite is what
        /// finally lands in Target, blended over whatever was already there
        /// (see PostProcess.hpp) using the scene's own coverage, so content
        /// from outside this call (2D sprites) is left alone. Settings comes
        /// from the scene's first PostProcessComponent, or defaults if it
        /// has none. Call between IRenderDevice::BeginFrame and EndFrame.
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

        /// @brief Builds the depth-only shadow pipeline. Optional - without
        /// it (or its shader) the scene just has no shadows.
        void CreateShadowPipeline(const PAK::AssetRegistry& Assets);

        /// @brief What the shadow pass hands the main pass: whether a shadow
        /// map was rendered this frame and how to look points up in it.
        struct ShadowState {
            bool Enabled {false};
            Float4x4 LightViewProjection {};
            Float4 Params {};   // see FrameData.hlsli's ShadowParams
            Float4 Params2 {};  // see FrameData.hlsli's ShadowParams2
        };

        /// @brief Renders every mesh actor into the shadow map from Light's
        /// point of view, into the current command buffer (before the main
        /// pass begins). The map is a single orthographic cascade fitted to
        /// the camera's view frustum out to the light's ShadowDistance, its
        /// origin snapped to whole texels so shadows don't shimmer as the
        /// camera moves, and its depth range widened to reach every caster
        /// between the light and that volume. Returns Enabled = false (and
        /// records nothing) when the light doesn't cast shadows, the camera
        /// isn't a perspective one, or nothing would cast.
        ShadowState RenderShadowPass(const Scene& S,
                                     const CameraComponent& Camera,
                                     const DirectionalLightComponent& Light,
                                     const Float3& LightDirection,
                                     MeshCache& Meshes);

        RHI::IRenderDevice* _Device {nullptr};

        RHI::LayoutHandle _Layout {};
        RHI::PipelineHandle _Pipeline {};
        RHI::PipelineHandle _SkyPipeline {};  // optional - see Initialize
        RHI::CommandBuffer _Commands;

        // Shared by every material texture slot (see MaterialBindings.hpp) -
        // one physical sampler bound repeatedly rather than one per channel,
        // since PBR maps all want the same tiling/filtering behavior.
        RHI::SamplerHandle _Sampler {};

        // Bound for whichever of a material's six channels has no map
        // assigned, so every draw always binds all six textures and the
        // shader never branches on "is this map present" - see PBR.hlsl.
        // White multiplies through as the identity for Albedo/Roughness/
        // Metallic/AmbientOcclusion/Emissive; flat-normal (0.5, 0.5, 1.0,
        // decoding to (0,0,1)) is the identity for Normal.
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

        // Directional-light shadows. _ShadowMap (D32_FLOAT, sampled + depth
        // target) is created on the first frame something needs it and
        // recreated if the light's ShadowResolution changes. The main
        // pipeline always declares the shadow slot, so a frame with no shadow
        // pass binds _ShadowFallback instead - a 1x1 map cleared to the far
        // plane, i.e. "nothing occludes" - and the shader's Enabled flag
        // skips the lookup anyway.
        RHI::LayoutHandle _ShadowLayout {};
        RHI::PipelineHandle _ShadowPipeline {};  // optional - see CreateShadowPipeline
        RHI::SamplerHandle _ShadowSampler {};
        RHI::TextureHandle _ShadowMap {};
        u32 _ShadowMapSize {0};
        RHI::TextureHandle _ShadowFallback {};

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

        // The scene render's own target: linear HDR (RGBA16F), sized to
        // match Target's color target and recreated if that changes (see
        // EnsureSceneColorTarget). Alpha marks "this pass wrote here" (1) vs
        // untouched (0) - see PostProcess.hpp.
        void EnsureSceneColorTarget(u32 Width, u32 Height);
        RHI::TextureHandle _SceneColorTarget {};
        u32 _SceneColorWidth {0};
        u32 _SceneColorHeight {0};

        PostProcess _PostProcess;
    };
}  // namespace Xen
