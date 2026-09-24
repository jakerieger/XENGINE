//
// Created by Jake Rieger on 9/18/2026.
//
// C++ mirror of Code/Shaders/Include/MaterialBindings.hlsli - see that file
// for the full rationale. Keep the two in sync by hand: a slot number here
// must equal the register index (the digit after b/t/s) that file's macro
// expands to.

#pragma once

#include <Common/XenCommon.hpp>

namespace Xen::MaterialSlot {
    // cbuffer slots (XEN_FRAME_REGISTER/XEN_OBJECT_REGISTER/
    // XEN_MATERIAL_REGISTER/XEN_LIGHT_REGISTER).
    inline constexpr u32 Frame    = 0;
    inline constexpr u32 Object   = 1;
    inline constexpr u32 Material = 2;

    // Every point/spot light in the scene (capped, see LightData.hlsli's
    // MAX_LIGHTS) - scene-level like the texture slots below, bound once per
    // frame, not per draw.
    inline constexpr u32 LightData = 3;

    // Forward+ tile-culling output (see LightCulling.hpp/.hlsli) - u0/u1, a
    // separate register space from the b#/t#/s# slots above so these numbers
    // don't collide with anything. Written by a compute pass earlier in the
    // frame, read here per-pixel; also bound (same slots) to LightCulling's
    // own compute pipeline layout.
    inline constexpr u32 LightIndexList = 0;  // u0
    inline constexpr u32 TileLightGrid  = 1;  // u1

    // Texture/sampler slots (XEN_*_TEX_REGISTER/XEN_*_SAMPLER_REGISTER) -
    // each is both the t# and the paired s# (see CommandBuffer::BindTexture).
    inline constexpr u32 Albedo           = 0;
    inline constexpr u32 Normal           = 1;
    inline constexpr u32 Roughness        = 2;  // single-channel (.r) - see MaterialBindings.hlsli
    inline constexpr u32 Metallic         = 3;  // single-channel (.r)
    inline constexpr u32 AmbientOcclusion = 4;
    inline constexpr u32 Emissive         = 5;

    // Scene-level slots: unlike the six above, these aren't a material's own
    // - they don't vary per draw, so MeshRenderer binds them once per frame,
    // before the per-actor loop, rather than once per draw.
    inline constexpr u32 Environment = 6;   // GGX-prefiltered environment cube, mip = roughness (RGBA16F)
    inline constexpr u32 Irradiance  = 7;   // cosine-convolved diffuse-lighting cube (RGBA16F)
    inline constexpr u32 BrdfLut     = 8;   // baked split-sum BRDF LUT (RG16F)
    inline constexpr u32 ShadowMap   = 9;   // directional-light shadow map (D32_FLOAT), comparison sampler
    inline constexpr u32 SSAO        = 10;  // screen-space ambient occlusion (R8_UNORM), sampled by screen UV - see SSAO.hpp

    /// @brief One past the highest texture slot above - the number of
    /// texture/sampler binding pairs a pipeline built on this convention
    /// declares. MeshRenderer binds slots [0, Environment) per draw and
    /// [Environment, TextureSlotCount) once per frame.
    inline constexpr u32 TextureSlotCount = 11;
}  // namespace Xen::MaterialSlot
