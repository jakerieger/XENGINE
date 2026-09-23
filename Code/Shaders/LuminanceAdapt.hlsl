// The last step of auto exposure's metering (see PostProcess.hpp): turns
// this frame's just-measured average scene luminance (the metering chain's
// final 1x1 level, still in log2 - see LuminanceMeasure.hlsl/
// LuminanceReduce.hlsl) into an eye-adaptation-smoothed value, blended from
// the previous frame's own adapted luminance so a sudden brightness change
// ramps in over AutoExposureAdaptUp/DownSeconds rather than snapping - the
// same reason a camera's auto exposure, or an eye's pupil, doesn't respond
// instantly. PostProcessComposite.hlsl reads the result back out to derive
// Exposure (Key / AvgLuminance).
//
// Loaded as a precompiled DXIL asset out of a pak (see PostProcess.cpp).

#include "Include/Fullscreen.hlsli"

cbuffer Params : register(b0) {
    // x = delta time (seconds); y = adapt-up rate in 1/seconds (used when
    // the scene measured brighter than the previous adapted value); z =
    // adapt-down rate (measured darker); w = 1 on the very first frame -
    // there's no previous frame to blend from yet, so jump straight to the
    // measured value instead of ramping from an arbitrary starting point.
    float4 Params;
};

Texture2D PrevAdaptedTex : register(t0);
SamplerState PrevAdaptedSampler : register(s0);

Texture2D MeasuredTex : register(t1);
SamplerState MeasuredSampler : register(s1);

VSOutput VSMain(uint VertexID : SV_VertexID) {
    return FullscreenVertex(VertexID, 0.0);
}

float PSMain(VSOutput In) : SV_Target {
    const float Measured = exp2(MeasuredTex.SampleLevel(MeasuredSampler, float2(0.5, 0.5), 0).r);
    if (Params.w > 0.5) return Measured;

    const float Prev  = PrevAdaptedTex.SampleLevel(PrevAdaptedSampler, float2(0.5, 0.5), 0).r;
    const float Rate  = Measured > Prev ? Params.y : Params.z;
    const float Blend = saturate(1.0 - exp(-Params.x * Rate));
    return lerp(Prev, Measured, Blend);
}
