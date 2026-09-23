// One step of GPU mip generation for an ordinary 2D texture (see
// MipGenerator.hpp / TextureCache::UploadEntry): a 4-tap box filter reading
// mip N-1, writing mip N of the same texture. Run once per mip above 0.
//
// No gamma-space handling here: an sRGB-formatted texture's SRV decodes to
// linear on sample and its RTV re-encodes to sRGB on write automatically
// (see MipGenerator's two pipelines, one per format) - averaging is correct
// in linear space, so the shader is identical either way.
//
// Loaded as a precompiled DXIL asset out of a pak (see MipGenerator.cpp).

#include "Include/Fullscreen.hlsli"

cbuffer Params : register(b0) {
    float4 TexelSize;  // xy = 1 / (source mip's width, height)
    float4 SourceMip;  // x = mip level to read from
};

Texture2D SourceTex : register(t0);
SamplerState SourceSampler : register(s0);

VSOutput VSMain(uint VertexID : SV_VertexID) {
    return FullscreenVertex(VertexID, 0.0);
}

float4 PSMain(VSOutput In) : SV_Target {
    // Four taps at a half-texel offset: hardware bilinear turns each into an
    // averaged 2x2 box, the same trick BloomDownsample.hlsl uses - one
    // texture read per corner instead of a wider explicit kernel.
    const float2 UV = In.UV;
    const float2 Texel = TexelSize.xy;
    float4 Sum = SourceTex.SampleLevel(SourceSampler, UV + float2(-0.5, -0.5) * Texel, SourceMip.x)
               + SourceTex.SampleLevel(SourceSampler, UV + float2(0.5, -0.5) * Texel, SourceMip.x)
               + SourceTex.SampleLevel(SourceSampler, UV + float2(-0.5, 0.5) * Texel, SourceMip.x)
               + SourceTex.SampleLevel(SourceSampler, UV + float2(0.5, 0.5) * Texel, SourceMip.x);
    return Sum * 0.25;
}
