// Screen-space ambient occlusion: estimates how occluded each pixel's
// hemisphere is by nearby geometry, from the depth buffer alone - a
// lightweight camera-space depth prepass (see MeshRenderer.cpp) runs before
// this, so a full scene depth already exists before any shading happens.
// This is a forward renderer with no G-buffer, so there's no per-pixel
// normal to read either; one is reconstructed from the depth buffer itself
// via screen-space derivatives.
//
// PBR.hlsl multiplies the (blurred - see SSAOBlur.hlsl) result into both the
// diffuse and specular IBL terms: unlike a directional light's shadow (see
// DirectionalLightComponent::ShadowAmbientDarkening), this models real
// geometric visibility of the environment, so it legitimately affects both.
//
// Loaded as a precompiled DXIL asset out of a pak (see SSAO.cpp).

#include "Include/Common.hlsli"
#include "Include/Fullscreen.hlsli"

cbuffer Params : register(b0) {
    row_major float4x4 ViewProjection;
    row_major float4x4 InvViewProjection;
    float4 CameraPositionAndPad;
    float4 Params2;  // x = radius (world units), y = power, z = bias (world units), w unused
};

Texture2D<float> DepthTex : register(t0);
SamplerState PointSampler : register(s0);

VSOutput VSMain(uint VertexID : SV_VertexID) {
    return FullscreenVertex(VertexID, 0.0);
}

// Fixed, not a runtime Settings field - same reasoning as the shadow map's
// 5x5 PCF kernel (PBR.hlsl): a compile-time trip count lets this unroll.
static const uint SampleCount = 16;

// Jorge Jimenez's interleaved gradient noise (see PBR.hlsl's shadow PCF,
// the same reason it's used here): rotates the sample kernel a different
// amount per pixel so the fixed 16-tap pattern reads as grain rather than a
// banded ring visible across a flat surface.
float InterleavedGradientNoise(float2 ScreenPos) {
    const float3 Magic = float3(0.06711056, 0.00583715, 52.9829189);
    return frac(Magic.z * frac(dot(ScreenPos, Magic.xy)));
}

float3 WorldPosFromDepth(float2 UV, float Depth) {
    const float2 Ndc   = float2(UV.x * 2.0 - 1.0, 1.0 - UV.y * 2.0);
    const float4 World = mul(float4(Ndc, Depth, 1.0), InvViewProjection);
    return World.xyz / World.w;
}

float4 PSMain(VSOutput In) : SV_Target {
    const float Depth = DepthTex.SampleLevel(PointSampler, In.UV, 0);
    if (Depth >= 1.0) return 1.0;  // the far plane / sky - nothing to occlude

    const float3 P = WorldPosFromDepth(In.UV, Depth);

    // A per-pixel normal from the depth buffer alone, via screen-space
    // derivatives of the reconstructed position - there's no G-buffer normal
    // to read in this forward renderer. A raw cross product's sign depends
    // on screen-space winding, which isn't worth reasoning about exactly:
    // just force the result to face the camera instead.
    float3 N = normalize(cross(ddx(P), ddy(P)));
    if (dot(N, CameraPositionAndPad.xyz - P) < 0.0) N = -N;

    const float Angle       = InterleavedGradientNoise(In.Position.xy) * (2.0 * PI);
    const float3 RandomVec  = float3(cos(Angle), sin(Angle), 0.0);
    const float3 T          = normalize(RandomVec - N * dot(RandomVec, N));
    const float3 B          = cross(N, T);
    const float3x3 TBN      = float3x3(T, B, N);

    const float Radius = Params2.x;
    const float Bias   = Params2.z;

    float Occlusion = 0.0;
    for (uint i = 0; i < SampleCount; ++i) {
        const float2 Xi = Hammersley(i, SampleCount);

        // Cosine-weighted hemisphere sample, local +Z = the surface normal -
        // the same formula IrradianceConvolve.hlsl uses for the same reason
        // (weights samples toward the normal, where an AO integral is most
        // sensitive, instead of wasting density at grazing angles).
        const float Phi       = 2.0 * PI * Xi.x;
        const float CosTheta  = sqrt(1.0 - Xi.y);
        const float SinTheta  = sqrt(Xi.y);
        const float3 LocalDir = float3(cos(Phi) * SinTheta, sin(Phi) * SinTheta, CosTheta);

        // Samples cluster closer to P than a uniform radius would - most of
        // a hemisphere's actual occluders are close to the surface, so this
        // spends more of the fixed sample budget where it resolves detail.
        const float T2    = (float(i) + 1.0) / float(SampleCount);
        const float Scale = lerp(0.1, 1.0, T2 * T2);

        const float3 SamplePos = P + mul(LocalDir, TBN) * Radius * Scale;

        const float4 SampleClip = mul(float4(SamplePos, 1.0), ViewProjection);
        const float3 SampleNdc  = SampleClip.xyz / SampleClip.w;
        const float2 SampleUV   = float2(SampleNdc.x * 0.5 + 0.5, 0.5 - SampleNdc.y * 0.5);
        if (any(SampleUV < 0.0) || any(SampleUV > 1.0)) continue;

        const float SceneDepth = DepthTex.SampleLevel(PointSampler, SampleUV, 0);
        const float3 SceneP    = WorldPosFromDepth(SampleUV, SceneDepth);

        const float DistToSample = length(CameraPositionAndPad.xyz - SamplePos);
        const float DistToScene  = length(CameraPositionAndPad.xyz - SceneP);

        // The real surface at SampleUV sits closer to the camera than the
        // virtual sample point - something occludes it. The range check
        // keeps unrelated, far-away geometry from counting: a wall well
        // outside Radius behind a small object shouldn't darken that object
        // just because it's technically "closer than infinity".
        const float RangeCheck = saturate(Radius / max(abs(DistToSample - DistToScene), 0.0001));
        Occlusion += (DistToScene < DistToSample - Bias) ? RangeCheck : 0.0;
    }

    const float AO = 1.0 - Occlusion / float(SampleCount);
    return pow(saturate(AO), max(Params2.y, 0.0001));
}
