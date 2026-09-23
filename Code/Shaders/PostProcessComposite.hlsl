// The last step of every frame's 3D rendering (see PostProcess.cpp): combines
// the linear HDR scene color with the bloom chain's fully upsampled result,
// applies exposure, then tonemaps and gamma-encodes (Include/Tonemap.hlsli)
// down to the swap chain's LDR format. Drawn as one fullscreen triangle,
// alpha-blended over whatever MeshRenderer's caller already drew into the
// target (see MeshRenderer.cpp) using SceneTex's own alpha, which is 1 only
// where the scene pass actually wrote a pixel (PBR.hlsl / Sky.hlsl) - so
// content from outside the 3D pass (2D sprites) is left alone.
//
// Loaded as a precompiled DXIL asset out of a pak (see MeshRenderer.cpp).

#include "Include/Fullscreen.hlsli"
#include "Include/Tonemap.hlsli"

cbuffer Params : register(b0) {
    float4 Params;   // x = manual exposure (used only while auto exposure is off), y = bloom intensity (0 disables it), z/w unused
    float4 Params2;  // x = auto exposure enabled (0/1), y = key value, z = metered-luminance min clamp, w = max clamp
};

Texture2D SceneTex : register(t0);
SamplerState SceneSampler : register(s0);

Texture2D BloomTex : register(t1);
SamplerState BloomSampler : register(s1);

// The auto-exposure metering chain's final, eye-adaptation-smoothed 1x1
// result (see PostProcess.hpp / LuminanceAdapt.hlsl) - always bound, same
// convention as BloomTex above (SceneTex again when auto exposure is
// unavailable, inert since Params2.x reads 0 then).
Texture2D AdaptedLuminanceTex : register(t2);
SamplerState AdaptedLuminanceSampler : register(s2);

VSOutput VSMain(uint VertexID : SV_VertexID) {
    return FullscreenVertex(VertexID, 0.0);
}

// Photographic "auto-key" exposure: scale so the metered average luminance
// lands on Params2.y (0.18 = "18% middle gray" by default), clamped first so
// one bright window or a momentarily black frame can't swing this to an
// extreme.
float ComputeExposure() {
    if (Params2.x < 0.5) return Params.x;

    const float AvgLum = clamp(
      AdaptedLuminanceTex.SampleLevel(AdaptedLuminanceSampler, float2(0.5, 0.5), 0).r, Params2.z, Params2.w);
    return Params2.y / max(AvgLum, 1e-4);
}

float4 PSMain(VSOutput In) : SV_Target {
    const float4 Scene = SceneTex.SampleLevel(SceneSampler, In.UV, 0);
    const float3 Bloom  = BloomTex.SampleLevel(BloomSampler, In.UV, 0).rgb;

    float3 Color = Scene.rgb + Bloom * Params.y;
    Color *= ComputeExposure();

    return float4(TonemapAndEncode(Color), Scene.a);
}
