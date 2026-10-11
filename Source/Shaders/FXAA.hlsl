// FXAA (Fast Approximate Anti-Aliasing) - Timothy Lottes' original NVIDIA
// algorithm (2009), the widely-circulated 4-tap "whitepaper appendix"
// version: a single fullscreen pass that estimates a local edge direction
// from the four diagonal neighbors' luma and blends along it - no geometry,
// multisampling or extra render targets involved. Runs last, on the fully
// composited LDR frame (see FXAA.cpp), which is exactly why it catches both
// a jagged silhouette edge and an aliased specular highlight the same way:
// by this point they're both just pixels with a sharp luma gradient,
// indistinguishable from each other to a filter that only looks at luma.
//
// This is a spatial-only filter - it smooths whatever a single frame looks
// like, but a highlight that's still genuinely aliasing will still flicker
// frame to frame, just with cleaner edges each time. Fixing that properly
// needs temporal accumulation (a planned follow-on, TAA), not this pass.
//
// Loaded as a precompiled DXIL asset out of a pak (see FXAA.cpp).

#include "Include/Fullscreen.hlsli"

cbuffer Params : register(b0) {
    float4 TexelSize;  // xy = 1 / SourceTex's size
};

Texture2D SourceTex : register(t0);
SamplerState SourceSampler : register(s0);

VSOutput VSMain(uint VertexID : SV_VertexID) {
    return FullscreenVertex(VertexID, 0.0);
}

float Luma(float3 Color) {
    // Perceptual luma weights - the input here is already gamma-encoded LDR
    // (this runs after PostProcessComposite's tonemap), matching what these
    // weights are calibrated for.
    return dot(Color, float3(0.299, 0.587, 0.114));
}

float3 Sample(float2 UV) {
    return SourceTex.SampleLevel(SourceSampler, UV, 0).rgb;
}

static const float ReduceMin = 1.0 / 128.0;
static const float ReduceMul = 1.0 / 8.0;
static const float SpanMax   = 8.0;

float4 PSMain(VSOutput In) : SV_Target {
    const float3 ColorTL = Sample(In.UV + float2(-1.0, -1.0) * TexelSize.xy);
    const float3 ColorTR = Sample(In.UV + float2(1.0, -1.0) * TexelSize.xy);
    const float3 ColorBL = Sample(In.UV + float2(-1.0, 1.0) * TexelSize.xy);
    const float3 ColorBR = Sample(In.UV + float2(1.0, 1.0) * TexelSize.xy);
    const float3 ColorM  = Sample(In.UV);

    const float LumaTL = Luma(ColorTL);
    const float LumaTR = Luma(ColorTR);
    const float LumaBL = Luma(ColorBL);
    const float LumaBR = Luma(ColorBR);
    const float LumaM  = Luma(ColorM);

    const float LumaMin = min(LumaM, min(min(LumaTL, LumaTR), min(LumaBL, LumaBR)));
    const float LumaMax = max(LumaM, max(max(LumaTL, LumaTR), max(LumaBL, LumaBR)));

    // Estimated edge direction straight from the diagonal luma gradient -
    // the whitepaper's core trick: an edge running roughly NE-SW makes the
    // TL/BR corners agree and TR/BL disagree (or vice versa for NW-SE),
    // which this turns directly into a blend direction without ever
    // explicitly classifying "horizontal vs vertical" the way a Sobel-style
    // filter would.
    float2 Dir;
    Dir.x = -((LumaTL + LumaTR) - (LumaBL + LumaBR));
    Dir.y = (LumaTL + LumaBL) - (LumaTR + LumaBR);

    // ReduceMin/Mul keep a near-flat area (where all four diagonals are
    // almost equal) from dividing by ~0 and picking a wild, noisy direction;
    // SpanMax caps how many texels the blend can reach so a strong, distant
    // edge doesn't smear across the whole neighborhood.
    const float DirReduce = max((LumaTL + LumaTR + LumaBL + LumaBR) * 0.25 * ReduceMul, ReduceMin);
    const float RcpDirMin = 1.0 / (min(abs(Dir.x), abs(Dir.y)) + DirReduce);
    Dir                   = clamp(Dir * RcpDirMin, -SpanMax, SpanMax) * TexelSize.xy;

    // Two blend estimates, cheapest first: RgbA is a 2-tap blend a third of
    // the way along Dir each side of center; RgbB widens that with the two
    // texels straddling center outright. RgbB looks better on a real edge
    // but can ring on a subtler one, so it's only trusted when its luma
    // actually falls within this neighborhood's own min/max - otherwise
    // fall back to the safer RgbA.
    const float3 RgbA = 0.5 * (Sample(In.UV + Dir * (1.0 / 3.0 - 0.5)) + Sample(In.UV + Dir * (2.0 / 3.0 - 0.5)));
    const float3 RgbB = RgbA * 0.5 + 0.25 * (Sample(In.UV + Dir * -0.5) + Sample(In.UV + Dir * 0.5));

    const float LumaB    = Luma(RgbB);
    const float3 Result  = (LumaB < LumaMin || LumaB > LumaMax) ? RgbA : RgbB;
    return float4(Result, 1.0);
}
