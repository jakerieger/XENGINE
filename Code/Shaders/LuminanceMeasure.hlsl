// First step of auto exposure's luminance-metering chain (see
// PostProcess.hpp / PostProcess.cpp's EnsureLuminanceChain): box-downsamples
// the HDR scene color to half its resolution while converting it to log2
// luminance in the same pass. Every level after this one
// (LuminanceReduce.hlsl) just averages already-log values down to 1x1,
// which LuminanceAdapt.hlsl reads back out with exp2 to recover the frame's
// metered average luminance.
//
// Loaded as a precompiled DXIL asset out of a pak (see PostProcess.cpp).

#include "Include/Fullscreen.hlsli"

cbuffer Params : register(b0) {
    float4 TexelSize;  // xy = 1 / SceneTex's size
};

Texture2D SceneTex : register(t0);
SamplerState SceneSampler : register(s0);

VSOutput VSMain(uint VertexID : SV_VertexID) {
    return FullscreenVertex(VertexID, 0.0);
}

float Luminance(float3 Color) {
    return dot(Color, float3(0.2126, 0.7152, 0.0722));
}

float3 Sample(float2 UV, float2 Offset) {
    return SceneTex.SampleLevel(SceneSampler, UV + Offset * TexelSize.xy, 0).rgb;
}

float PSMain(VSOutput In) : SV_Target {
    // Same 4-tap half-texel-offset box as BloomDownsample.hlsl - average the
    // linear luminance first, log2 it once, rather than averaging four logs:
    // a true box-filtered mean at this first level, not yet the
    // geometric-mean approximation every Reduce level after it settles for.
    const float AvgLum = 0.25 * (Luminance(Sample(In.UV, float2(-0.5, -0.5))) +
                                 Luminance(Sample(In.UV, float2(0.5, -0.5))) +
                                 Luminance(Sample(In.UV, float2(-0.5, 0.5))) +
                                 Luminance(Sample(In.UV, float2(0.5, 0.5))));
    return log2(max(AvgLum, 1e-4));
}
