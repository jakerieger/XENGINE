//
// Created by Jake Rieger on 9/24/2026.
//
// C++ mirror of Code/Shaders/Include/LightData.hlsli - shared by
// MeshRenderer (which collects every scene light into this shape once per
// frame) and LightCulling (which reads the same cbuffer to test lights
// against a tile's bounds). Kept in one place, not duplicated per-.cpp, so
// there's exactly one byte-layout-sensitive mirror to keep in sync with the
// HLSL side by hand.

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Math.hpp>

namespace Xen {
    constexpr f32 LightTypePoint = 0.0f;
    constexpr f32 LightTypeSpot  = 1.0f;

    // Matches Code/Shaders/Include/LightData.hlsli's Light exactly.
    struct GpuLight {
        Float4 PositionAndRange;
        Float4 ColorAndIntensity;
        Float4 DirectionAndType;
        Float4 ConeAnglesAndPad;
    };

    // One past the highest light index LightData.hlsli's fixed-size array
    // holds (XEN_MAX_LIGHTS) - kept in sync by hand, same no-shared-codegen
    // tradeoff as every other HLSL/C++ mirrored struct in this engine.
    constexpr u32 MaxLights = 128;

    // Matches Code/Shaders/Include/LightData.hlsli's cbuffer exactly,
    // including its 16-byte-aligned header (LightCount + 3 pad words,
    // matching a single float4 slot the same way HLSL packs it).
    struct LightConstants {
        u32 LightCount {0};
        u32 Pad[3] {};
        GpuLight Lights[MaxLights] {};
    };
}  // namespace Xen
