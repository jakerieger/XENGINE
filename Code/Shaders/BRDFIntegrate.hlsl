// Bakes the split-sum BRDF lookup table (Karis, "Real Shading in Unreal
// Engine 4", SIGGRAPH 2013) that PBR.hlsl's indirect specular samples: for a
// given (NdotV, Roughness), the integral of the GGX specular BRDF against a
// uniform white environment, split into a scale and a bias on F0 -
// F0 * Scale + Bias reproduces the environment BRDF for any Fresnel
// reflectance, so one 2D texture covers every material.
//
// Drawn exactly once, at MeshRenderer::Initialize (see BakeBrdfLut), as a
// single fullscreen triangle into a 256x256 RG16F render target - the
// texture's U axis is NdotV, its V axis is Roughness, matching the
// float2(NdotV, Roughness) lookup PBR.hlsl does. No bindings at all: the
// integration is a pure function of the pixel's own position.

#include "Include/Common.hlsli"
#include "Include/Fullscreen.hlsli"

// Enough samples that the LUT is smooth (the reference implementations use
// 1024); it's baked once, so cost here is a one-time startup expense, not a
// per-frame one.
static const uint SampleCount = 1024;

VSOutput VSMain(uint VertexID : SV_VertexID) {
    return FullscreenVertex(VertexID, 0.0);
}

// Smith geometry term with the IBL remapping of k (Roughness^2 / 2, not the
// direct-lighting (Roughness + 1)^2 / 8 PBR.hlsl uses for analytic lights).
float GeometrySchlickGGXIBL(float NdotX, float Roughness) {
    const float K = (Roughness * Roughness) / 2.0;
    return NdotX / (NdotX * (1.0 - K) + K);
}

float GeometrySmithIBL(float NdotV, float NdotL, float Roughness) {
    return GeometrySchlickGGXIBL(NdotV, Roughness) * GeometrySchlickGGXIBL(NdotL, Roughness);
}

float2 IntegrateBRDF(float NdotV, float Roughness) {
    const float3 V = float3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);

    float Scale = 0.0;
    float Bias  = 0.0;

    for (uint i = 0; i < SampleCount; ++i) {
        const float2 Xi = Hammersley(i, SampleCount);
        const float3 H  = ImportanceSampleGGX(Xi, Roughness);
        const float3 L  = normalize(2.0 * dot(V, H) * H - V);

        const float NdotL = saturate(L.z);
        const float NdotH = saturate(H.z);
        const float VdotH = saturate(dot(V, H));

        if (NdotL > 0.0) {
            const float G    = GeometrySmithIBL(NdotV, NdotL, Roughness);
            const float GVis = (G * VdotH) / max(NdotH * NdotV, 0.0001);
            const float Fc   = pow(1.0 - VdotH, 5.0);

            Scale += (1.0 - Fc) * GVis;
            Bias  += Fc * GVis;
        }
    }

    return float2(Scale, Bias) / float(SampleCount);
}

float2 PSMain(VSOutput In) : SV_Target {
    // NdotV of exactly 0 divides by zero in the visibility term; nothing
    // ever samples that column meaningfully (a surface seen edge-on).
    return IntegrateBRDF(max(In.UV.x, 0.001), In.UV.y);
}
