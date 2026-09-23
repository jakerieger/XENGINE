// Denoises SSAO.hlsl's raw per-pixel output (16 samples, dithered - still
// visibly grainy on its own) with a small 3x3 box blur. Not depth- or
// normal-aware, so it can bleed occlusion slightly across a sharp depth
// edge - a plain blur is the simplest thing that removes the dither grain,
// and this engine has nothing sharper to compare it against yet; a
// bilateral (depth-weighted) blur would be the next step up if edge
// bleeding ever becomes visible in practice.
//
// Loaded as a precompiled DXIL asset out of a pak (see SSAO.cpp).

#include "Include/Fullscreen.hlsli"

cbuffer Params : register(b0) {
    float4 TexelSize;  // xy = 1 / SourceTex's size
};

Texture2D<float> SourceTex : register(t0);
SamplerState SourceSampler : register(s0);

VSOutput VSMain(uint VertexID : SV_VertexID) {
    return FullscreenVertex(VertexID, 0.0);
}

float4 PSMain(VSOutput In) : SV_Target {
    float Sum = 0.0;
    [unroll] for (int Y = -1; Y <= 1; ++Y) {
        [unroll] for (int X = -1; X <= 1; ++X) {
            Sum += SourceTex.SampleLevel(SourceSampler, In.UV + float2(X, Y) * TexelSize.xy, 0);
        }
    }
    return Sum / 9.0;
}
