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
    // XEN_MATERIAL_REGISTER).
    inline constexpr u32 Frame    = 0;
    inline constexpr u32 Object   = 1;
    inline constexpr u32 Material = 2;

    // Texture/sampler slots (XEN_*_TEX_REGISTER/XEN_*_SAMPLER_REGISTER) -
    // each is both the t# and the paired s# (see CommandBuffer::BindTexture).
    inline constexpr u32 Albedo            = 0;
    inline constexpr u32 Normal            = 1;
    inline constexpr u32 MetallicRoughness = 2;
    inline constexpr u32 AmbientOcclusion  = 3;
    inline constexpr u32 Emissive          = 4;

    // Scene-level slots: unlike the five above, these aren't a material's own
    // - they don't vary per draw, so MeshRenderer binds them once per frame,
    // before the per-actor loop, rather than once per draw.
    inline constexpr u32 Environment = 5;  // GGX-prefiltered equirect environment, mip = roughness (RGBA16F)
    inline constexpr u32 Irradiance  = 6;  // cosine-convolved equirect diffuse lighting (RGBA16F)
    inline constexpr u32 BrdfLut     = 7;  // baked split-sum BRDF LUT (RG16F)

    /// @brief One past the highest texture slot above - the number of
    /// texture/sampler binding pairs a pipeline built on this convention
    /// declares. MeshRenderer binds slots [0, Environment) per draw and
    /// [Environment, TextureSlotCount) once per frame.
    inline constexpr u32 TextureSlotCount = 8;
}  // namespace Xen::MaterialSlot
