// One halving step of auto exposure's luminance-metering chain (see
// LuminanceMeasure.hlsl for level 0, PostProcess.cpp's EnsureLuminanceChain
// for the chain itself): box-averages four already-log2-luminance texels
// from the previous, twice-as-large level down into this one. Repeated
// until the chain reaches exactly 1x1 (see LuminanceAdapt.hlsl). Averaging
// already-logged values approximates the geometric mean of the region each
// output texel covers, rather than the true (linear) mean - the standard,
// cheap approximation for this, and the reason it matters at all: one very
// bright window or light source doesn't get to dominate the metered average
// the way it would under a straight linear box filter.
//
// Loaded as a precompiled DXIL asset out of a pak (see PostProcess.cpp).

#include "Include/Fullscreen.hlsli"

cbuffer Params : register(b0) {
    float4 TexelSize;  // xy = 1 / SourceTex's size
};

Texture2D SourceTex : register(t0);
SamplerState SourceSampler : register(s0);

VSOutput VSMain(uint VertexID : SV_VertexID) {
    return FullscreenVertex(VertexID, 0.0);
}

float Sample(float2 UV, float2 Offset) {
    return SourceTex.SampleLevel(SourceSampler, UV + Offset * TexelSize.xy, 0).r;
}

float PSMain(VSOutput In) : SV_Target {
    return 0.25 * (Sample(In.UV, float2(-0.5, -0.5)) + Sample(In.UV, float2(0.5, -0.5)) +
                   Sample(In.UV, float2(-0.5, 0.5)) + Sample(In.UV, float2(0.5, 0.5)));
}
