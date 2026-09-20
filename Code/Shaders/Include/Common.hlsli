// Shader code shared by more than one engine shader: constants, the
// low-discrepancy sampling helpers the IBL bake passes use, the
// direction <-> texture-coordinate mappings the bakers and PBR.hlsl must agree
// on, and the roughness <-> mip mapping of the prefiltered environment cube.
// Include with `#include "Include/Common.hlsli"` from a shader in Code/Shaders.

#ifndef XEN_COMMON_HLSLI
#define XEN_COMMON_HLSLI

static const float PI = 3.14159265359;

// --- Environment source (equirectangular) ------------------------------------

// Direction -> equirectangular UV. The image's center (U = 0.5) is the
// engine's forward direction (-Z, glTF's convention), +X is at U = 0.75, and
// the top row (V = 0) is straight up (+Y) - Scripts/generate_test_hdri.py
// writes its test map with exactly this mapping, so keep the two in sync.
//
// Only the bake passes use this: an environment is authored as an equirect
// image, but everything PBR.hlsl and Sky.hlsl sample is a baked cube map.
float2 DirToEquirectUV(float3 Dir) {
    const float U = atan2(Dir.x, -Dir.z) / (2.0 * PI) + 0.5;
    const float V = 0.5 - asin(clamp(Dir.y, -1.0, 1.0)) / PI;
    return float2(U, V);
}

// Solid angle of one texel of an equirect image of the given size, for a
// direction at the given latitude: texels are 2*PI/W wide and PI/H tall in
// angle, but a row's real width shrinks with cos(latitude) toward the poles.
// Using the image-wide average instead over- or under-filters the source
// depending on latitude. (cos(latitude) == sqrt(1 - Dir.y^2).)
float EquirectTexelSolidAngle(float3 Dir, float Width, float Height) {
    const float CosLatitude = sqrt(max(1.0 - Dir.y * Dir.y, 0.0001));
    return (2.0 * PI / Width) * (PI / Height) * CosLatitude;
}

// --- Cube maps ---------------------------------------------------------------

// Direction for texel UV (0..1, (0,0) top-left, as rendered by Fullscreen.hlsli)
// of face Face - the inverse of how D3D selects a cube face and texel from a
// direction (major axis picks the face, the other two axes become s/t), with
// faces in D3D's order: +X, -X, +Y, -Y, +Z, -Z. The bake passes write with
// this; hardware cube sampling reads it back, so the two must agree.
float3 CubeFaceUVToDir(uint Face, float2 UV) {
    const float U = UV.x * 2.0 - 1.0;
    const float V = UV.y * 2.0 - 1.0;

    float3 Dir;
    switch (Face) {
        case 0:  Dir = float3(1.0, -V, -U); break;   // +X
        case 1:  Dir = float3(-1.0, -V, U); break;   // -X
        case 2:  Dir = float3(U, 1.0, V); break;     // +Y
        case 3:  Dir = float3(U, -1.0, -V); break;   // -Y
        case 4:  Dir = float3(U, -V, 1.0); break;    // +Z
        default: Dir = float3(-U, -V, -1.0); break;  // -Z
    }
    return normalize(Dir);
}

// --- Prefiltered environment: roughness <-> mip -------------------------------

// Mip 0 is a mirror; each level above it is baked with a roughness that halves
// the lobe's angular width every 1/ROUGHNESS_MIP_SCALE mips, so the blur grows
// as fast as the texels do (each mip's texels are twice the size of the one
// below) instead of a linear roughness ramp that spends most of its mips on
// nearly-identical mirror-sharp levels. Level Top = Levels - 1 is roughness 1.
static const float ROUGHNESS_MIP_SCALE = 1.2;

// The roughness the prefilter bakes mip Mip of Levels with.
float RoughnessForMip(uint Mip, uint Levels) {
    if (Mip == 0) return 0.0;
    return exp2(-float(Levels - 1 - Mip) / ROUGHNESS_MIP_SCALE);
}

// The (fractional) mip PBR.hlsl samples for a surface roughness - the exact
// inverse of RoughnessForMip, so a roughness equal to a baked level's lands
// exactly on that level.
float MipForRoughness(float Roughness, uint Levels) {
    const float Top = float(Levels) - 1.0;
    return clamp(Top + ROUGHNESS_MIP_SCALE * log2(max(Roughness, 0.0001)), 0.0, Top);
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
