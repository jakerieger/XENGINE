// Draws the scene's environment as a background: one fullscreen triangle at
// the far plane, so it only shows where no mesh has been drawn (the depth test
// is LessEqual against a buffer cleared to 1.0), reading the baked
// environment cube map along each pixel's world-space view ray.
//
// Shares PBR.hlsl's pipeline layout exactly (see MeshRenderer.cpp) - same b0
// FrameData, same t5/s5 environment slot - so MeshRenderer switches to it
// mid-pass with every binding already in place. Mip 0 of the prefiltered cube
// is the mirror level: the source itself, undegraded by any lobe.

#include "Include/Fullscreen.hlsli"
#include "Include/FrameData.hlsli"

TextureCube EnvironmentMap : register(XEN_ENVIRONMENT_TEX_REGISTER);
SamplerState EnvironmentSampler : register(XEN_ENVIRONMENT_SAMPLER_REGISTER);

VSOutput VSMain(uint VertexID : SV_VertexID) {
    return FullscreenVertex(VertexID, 1.0);
}

float4 PSMain(VSOutput In) : SV_Target {
    // Unproject this pixel's far-plane point back to world space; the view
    // ray is from the camera through it.
    const float2 Ndc   = float2(In.UV.x * 2.0 - 1.0, 1.0 - In.UV.y * 2.0);
    const float4 World = mul(float4(Ndc, 1.0, 1.0), InvViewProjection);
    const float3 Dir   = normalize(World.xyz / World.w - CameraPositionAndPad.xyz);

    // Linear HDR, same as PBR.hlsl - the post-process composite pass is what
    // exposes and tonemaps the sky and every lit surface together.
    const float3 Radiance = EnvironmentMap.SampleLevel(EnvironmentSampler, Dir, 0).rgb;
    return float4(Radiance, 1.0);
}
