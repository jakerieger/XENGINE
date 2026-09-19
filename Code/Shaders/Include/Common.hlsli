// Shader code shared by more than one engine shader: constants, the
// low-discrepancy sampling helpers the IBL bake passes use, and the
// direction <-> equirectangular mapping PBR.hlsl and the bakers must agree on.
// Include with `#include "Include/Common.hlsli"` from a shader in Code/Shaders.

#ifndef XEN_COMMON_HLSLI
#define XEN_COMMON_HLSLI

static const float PI = 3.14159265359;

// --- Equirectangular mapping -------------------------------------------------

// Direction -> equirectangular UV. The image's center (U = 0.5) is the
// engine's forward direction (-Z, glTF's convention), +X is at U = 0.75, and
// the top row (V = 0) is straight up (+Y) - Scripts/generate_test_hdri.py
// writes its test map with exactly this mapping, so keep the two in sync.
float2 DirToEquirectUV(float3 Dir) {
    const float U = atan2(Dir.x, -Dir.z) / (2.0 * PI) + 0.5;
    const float V = 0.5 - asin(clamp(Dir.y, -1.0, 1.0)) / PI;
    return float2(U, V);
}

// The exact inverse of DirToEquirectUV.
float3 EquirectUVToDir(float2 UV) {
    const float Theta = (UV.x - 0.5) * 2.0 * PI;
    const float Phi   = (0.5 - UV.y) * PI;
    const float CosPhi = cos(Phi);
    return float3(CosPhi * sin(Theta), sin(Phi), -CosPhi * cos(Theta));
}

// --- Sampling --------------------------------------------------------------

float RadicalInverseVdC(uint Bits) {
    Bits = (Bits << 16u) | (Bits >> 16u);
    Bits = ((Bits & 0x55555555u) << 1u) | ((Bits & 0xAAAAAAAAu) >> 1u);
    Bits = ((Bits & 0x33333333u) << 2u) | ((Bits & 0xCCCCCCCCu) >> 2u);
    Bits = ((Bits & 0x0F0F0F0Fu) << 4u) | ((Bits & 0xF0F0F0F0u) >> 4u);
    Bits = ((Bits & 0x00FF00FFu) << 8u) | ((Bits & 0xFF00FF00u) >> 8u);
    return float(Bits) * 2.3283064365386963e-10;  // 1 / 2^32
}

float2 Hammersley(uint i, uint N) {
    return float2(float(i) / float(N), RadicalInverseVdC(i));
}

// Halfway vector distributed by the GGX normal distribution, in the local
// frame where the surface normal is +Z. Callers that need it in another frame
// build their own tangent basis (see PrefilterEnvironment.hlsl).
float3 ImportanceSampleGGX(float2 Xi, float Roughness) {
    const float A = Roughness * Roughness;

    const float Phi      = 2.0 * PI * Xi.x;
    const float CosTheta = sqrt((1.0 - Xi.y) / (1.0 + (A * A - 1.0) * Xi.y));
    const float SinTheta = sqrt(1.0 - CosTheta * CosTheta);

    return float3(cos(Phi) * SinTheta, sin(Phi) * SinTheta, CosTheta);
}

#endif  // XEN_COMMON_HLSLI
