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
    // averaged 2x2 box, so this is effectively an 4x4-texel box filter for
    // one texture read per corner - cheap, and stable under motion (no
    // single-texel fireflies the way a naive 1-tap downsample would have).
    float3 Color = Sample(In.UV, float2(-0.5, -0.5)) + Sample(In.UV, float2(0.5, -0.5)) +
                    Sample(In.UV, float2(-0.5, 0.5)) + Sample(In.UV, float2(0.5, 0.5));
    Color *= 0.25;

    if (Threshold.z > 0.5) Color = SoftThreshold(Color);

    return float4(Color, 1.0);
}
