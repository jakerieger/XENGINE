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

    /// @brief One past the highest texture slot above - the number of
    /// texture/sampler binding pairs a pipeline built on this convention
    /// declares, and the number MeshRenderer always binds per draw.
    inline constexpr u32 TextureSlotCount = 5;
}  // namespace Xen::MaterialSlot
