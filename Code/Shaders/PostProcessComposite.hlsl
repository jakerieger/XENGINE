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
    float4 Params;  // x = exposure, y = bloom intensity (0 disables it), z/w unused
};

Texture2D SceneTex : register(t0);
SamplerState SceneSampler : register(s0);

Texture2D BloomTex : register(t1);
SamplerState BloomSampler : register(s1);

VSOutput VSMain(uint VertexID : SV_VertexID) {
    return FullscreenVertex(VertexID, 0.0);
}

float4 PSMain(VSOutput In) : SV_Target {
    const float4 Scene = SceneTex.SampleLevel(SceneSampler, In.UV, 0);
    const float3 Bloom  = BloomTex.SampleLevel(BloomSampler, In.UV, 0).rgb;

    float3 Color = Scene.rgb + Bloom * Params.y;
    Color *= Params.x;

    return float4(TonemapAndEncode(Color), Scene.a);
}
