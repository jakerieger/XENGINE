// One step of the bloom mip chain's upsample half (see PostProcess.hpp): a
// 3x3 tent-filtered sample of the smaller mip (SourceMip), additively blended
// (see the pipeline's blend state, not this shader) onto the next mip up,
// which already holds that level's own downsampled content. Repeated from
// the smallest level back to mip 0 turns the chain into a single
// progressively-wider blur - the "physically based bloom" technique
// popularized by Call of Duty: Advanced Warfare's SIGGRAPH 2014 presentation.
//
// Loaded as a precompiled DXIL asset out of a pak (see PostProcess.cpp).

#include "Include/Fullscreen.hlsli"

cbuffer Params : register(b0) {
    float4 TexelSize;  // xy = 1 / (SourceTex's size at SourceMip, the smaller level being upsampled)
    float4 Params2;    // x = SourceMip
};

Texture2D SourceTex : register(t0);
SamplerState SourceSampler : register(s0);

VSOutput VSMain(uint VertexID : SV_VertexID) {
    return FullscreenVertex(VertexID, 0.0);
}

float3 Sample(float2 UV, float2 Offset) {
    return SourceTex.SampleLevel(SourceSampler, UV + Offset * TexelSize.xy, Params2.x).rgb;
}

float4 PSMain(VSOutput In) : SV_Target {
    // 3x3 tent: center weight 4, edge-adjacent weight 2, corner weight 1,
    // over 16 - the standard kernel for this technique (wide enough to hide
    // the seams a plain bilinear upsample leaves between mip levels).
    float3 Color = Sample(In.UV, float2(-1.0, -1.0)) + 2.0 * Sample(In.UV, float2(0.0, -1.0)) +
                    Sample(In.UV, float2(1.0, -1.0)) + 2.0 * Sample(In.UV, float2(-1.0, 0.0)) +
                    4.0 * Sample(In.UV, float2(0.0, 0.0)) + 2.0 * Sample(In.UV, float2(1.0, 0.0)) +
                    Sample(In.UV, float2(-1.0, 1.0)) + 2.0 * Sample(In.UV, float2(0.0, 1.0)) +
                    Sample(In.UV, float2(1.0, 1.0));
    Color /= 16.0;

    // Alpha 1: the blend state's SrcAlpha factor is what scales this add
    // (BlendAttachmentState::Additive - see PostProcess.cpp), so it has to
    // be full-strength here rather than a second, redundant intensity knob.
    return float4(Color, 1.0);
}
