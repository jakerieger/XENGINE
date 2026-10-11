// Denoises SSAO.hlsl's raw per-pixel output (16 samples, dithered - still
// visibly grainy on its own) with a depth-aware (bilateral) 3x3 blur: each
// tap is weighted by how close its depth is to the center pixel's, so a
// neighbor that's actually a different, unrelated surface (a sharp depth
// edge one texel away) contributes little to nothing instead of bleeding
// its occlusion across the edge - what a plain, uniformly-weighted box blur
// couldn't avoid. Not normal-aware (two coplanar-in-depth but
// differently-oriented surfaces still blend) - depth alone has covered every
// edge-bleeding case seen in practice so far.
//
// The weight compares raw NDC depth (SampleLevel straight off the R32/D32
// depth target), not linear/world-space depth: NDC depth's well-known
// non-linearity only distorts a comparison across a WIDE spread of samples,
// and this kernel is exactly one texel in every direction - on the same
// smooth surface that's a near-zero NDC delta regardless of distance from
// the camera, while a real edge is proportionally far larger, so the cheap
// direct comparison is enough without reconstructing world position.
//
// Loaded as a precompiled DXIL asset out of a pak (see SSAO.cpp).

#include "Include/Fullscreen.hlsli"

cbuffer Params : register(b0) {
    float4 TexelSize;  // xy = 1 / SourceTex's size
};

Texture2D<float> SourceTex : register(t0);
Texture2D<float> DepthTex  : register(t1);
SamplerState SourceSampler : register(s0);  // point/clamp - reused for both textures, see SSAO.cpp

VSOutput VSMain(uint VertexID : SV_VertexID) {
    return FullscreenVertex(VertexID, 0.0);
}

// How much a tap's NDC depth may differ from the center pixel's before its
// weight falls to zero - a fixed constant, not a Settings knob, the same
// "simplest thing that works" choice SampleCount (SSAO.hlsl) already made.
// Tuned against this engine's default near/far/FOV: comfortably wider than
// the sub-1e-4 jitter between adjacent pixels on one smooth surface, tight
// enough to reject an actual foreground/background edge.
static const float DepthEdgeThreshold = 0.001;

float4 PSMain(VSOutput In) : SV_Target {
    const float CenterDepth = DepthTex.SampleLevel(SourceSampler, In.UV, 0);

    float Sum        = 0.0;
    float WeightSum   = 0.0;
    [unroll] for (int Y = -1; Y <= 1; ++Y) {
        [unroll] for (int X = -1; X <= 1; ++X) {
            const float2 SampleUV = In.UV + float2(X, Y) * TexelSize.xy;
            const float TapDepth  = DepthTex.SampleLevel(SourceSampler, SampleUV, 0);

            // Linear falloff to 0 at DepthEdgeThreshold - same "plain,
            // explainable" shape as SSAO.hlsl's own RangeCheck, not a
            // Gaussian: one threshold to reason about instead of a sigma.
            const float DepthDiff = abs(TapDepth - CenterDepth);
            const float Weight    = saturate(1.0 - DepthDiff / DepthEdgeThreshold);

            Sum += SourceTex.SampleLevel(SourceSampler, SampleUV, 0) * Weight;
            WeightSum += Weight;
        }
    }

    // The center tap (X=0,Y=0) always has Weight=1 (DepthDiff=0), so
    // WeightSum is never actually 0 - the epsilon guard is a formality, not
    // a real case this hits.
    return Sum / max(WeightSum, 0.0001);
}
