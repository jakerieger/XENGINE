// One step of the bloom mip chain's downsample half (see PostProcess.hpp):
// box-filters SourceTex down to half its size, into the next mip of the same
// chain texture (level 0's source is the HDR scene instead - see the
// SourceMip param). Level 0 also extracts a soft-thresholded bright pass, so
// only highlights above the threshold seed the chain; every level after it
// re-filters what's already been extracted, so the threshold runs once, not
// once per level.
//
// Loaded as a precompiled DXIL asset out of a pak (see PostProcess.cpp).

#include "Include/Fullscreen.hlsli"

cbuffer Params : register(b0) {
    float4 TexelSize;  // xy = 1 / (SourceTex's size at SourceMip)
    float4 Threshold;  // x = threshold, y = soft-knee width, z = 1 to threshold (level 0) else 0, w = SourceMip
};

Texture2D SourceTex : register(t0);
SamplerState SourceSampler : register(s0);

VSOutput VSMain(uint VertexID : SV_VertexID) {
    return FullscreenVertex(VertexID, 0.0);
}

float3 Sample(float2 UV, float2 Offset) {
    return SourceTex.SampleLevel(SourceSampler, UV + Offset * TexelSize.xy, Threshold.w).rgb;
}

float Luminance(float3 Color) {
    return dot(Color, float3(0.2126, 0.7152, 0.0722));
}

// Brian Karis's "partial Karis average" (Unreal / the same SIGGRAPH 2014
// mobile-bloom talk this whole technique comes from): weight each sample by
// 1/(1+luminance) before averaging, so one extreme outlier - an aliased,
// sub-pixel-narrow specular highlight, exactly what a rotating metallic
// object with no anti-aliasing produces - contributes almost nothing to the
// sum instead of dominating a plain average. A plain box filter is stable
// against single-texel noise in general, but not against a genuine firefly:
// (1000+1+1+1)/4 is still ~250, so that whole block blooms - and as the
// highlight's exact texel shifts frame to frame under rotation, which block
// crosses threshold shifts with it, reading as sparkling. Only applied at
// level 0, the one read straight from the raw HDR scene - every level after
// re-filters already-smoothed chain data, which doesn't need it.
float3 KarisAverage(float3 C0, float3 C1, float3 C2, float3 C3) {
    const float W0 = 1.0 / (1.0 + Luminance(C0));
    const float W1 = 1.0 / (1.0 + Luminance(C1));
    const float W2 = 1.0 / (1.0 + Luminance(C2));
    const float W3 = 1.0 / (1.0 + Luminance(C3));
    return (C0 * W0 + C1 * W1 + C2 * W2 + C3 * W3) / max(W0 + W1 + W2 + W3, 1e-5);
}

// Unity's built-in bloom curve: a quadratic knee below Threshold, a hard
// pass-through above it, continuous (and continuously differentiable) at the
// crossover so there's no visible seam where a highlight's edge crosses it.
float3 SoftThreshold(float3 Color) {
    const float ThresholdValue = Threshold.x;
    const float Knee           = Threshold.x * Threshold.y + 1e-5;

    const float Brightness = max(Color.r, max(Color.g, Color.b));
    float Soft              = Brightness - ThresholdValue + Knee;
    Soft                     = clamp(Soft, 0.0, 2.0 * Knee);
    Soft                     = Soft * Soft / (4.0 * Knee + 1e-5);

    const float Contribution = max(Soft, Brightness - ThresholdValue) / max(Brightness, 1e-5);
    return Color * Contribution;
}

float4 PSMain(VSOutput In) : SV_Target {
    // Four taps at a half-texel offset: hardware bilinear turns each into an
    // averaged 2x2 box, so this is effectively a 4x4-texel box filter for
    // one texture read per corner.
    const float3 S0 = Sample(In.UV, float2(-0.5, -0.5));
    const float3 S1 = Sample(In.UV, float2(0.5, -0.5));
    const float3 S2 = Sample(In.UV, float2(-0.5, 0.5));
    const float3 S3 = Sample(In.UV, float2(0.5, 0.5));

    const bool IsLevel0 = Threshold.z > 0.5;
    float3 Color        = IsLevel0 ? KarisAverage(S0, S1, S2, S3) : (S0 + S1 + S2 + S3) * 0.25;

    if (IsLevel0) Color = SoftThreshold(Color);

    return float4(Color, 1.0);
}
