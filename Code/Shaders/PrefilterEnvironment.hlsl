// Bakes one roughness level of the prefiltered specular environment map: each
// output texel is the source environment convolved with the GGX specular lobe
// for that texel's direction, so a glossy surface can read a blurry
// reflection with a single texture sample instead of integrating the
// hemisphere per pixel. Drawn once per mip of the destination (roughness =
// mip / (mips - 1)) by EnvironmentBaker, each into its own render-target mip.
//
// Split-sum approximation: N = V = R (the standard assumption that lets one
// texture cover every view angle), so the lobe is symmetric around the
// output direction. Source samples are taken from the source's mip pyramid
// at a level chosen from each sample's pdf (Krivanek & Colbert, "Real-time
// Shading with Filtered Importance Sampling") - without that, a very bright
// small feature like a sun disc turns into speckle at low sample counts.

#include "Include/Common.hlsli"
#include "Include/Fullscreen.hlsli"

cbuffer BakeParams : register(b0) {
    float4 Params;  // x = Roughness, y = SampleCount, z = destination width
};

Texture2D SourceMap : register(t0);
SamplerState SourceSampler : register(s0);

float DistributionGGX(float NdotH, float Roughness) {
    const float A     = Roughness * Roughness;
    const float A2    = A * A;
    const float Denom = NdotH * NdotH * (A2 - 1.0) + 1.0;
    return A2 / max(PI * Denom * Denom, 0.0000001);
}

float4 PSMain(VSOutput In) : SV_Target {
    uint SourceWidth, SourceHeight, SourceLevels;
    SourceMap.GetDimensions(0, SourceWidth, SourceHeight, SourceLevels);

    const float Roughness   = Params.x;
    const uint SampleCount  = (uint) Params.y;
    const float3 N          = EquirectUVToDir(In.UV);

    // Roughness 0 is a mirror: no lobe to integrate, just the source itself,
    // filtered down if the destination is smaller than the source.
    if (Roughness < 0.001) {
        const float Lod = max(log2(float(SourceWidth) / Params.z), 0.0);
        return float4(SourceMap.SampleLevel(SourceSampler, In.UV, Lod).rgb, 1.0);
    }

    const float3 Up = abs(N.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    const float3 T  = normalize(cross(Up, N));
    const float3 B  = cross(N, T);

    // Average solid angle of one source texel; equirect texels shrink toward
    // the poles, so this overestimates the LOD there - the cost of not
    // weighting by sin(theta), acceptable for a filter bias.
    const float SaTexel = 4.0 * PI / (float(SourceWidth) * float(SourceHeight));

    float3 Color = float3(0.0, 0.0, 0.0);
    float Weight = 0.0;

    for (uint i = 0; i < SampleCount; ++i) {
        const float2 Xi = Hammersley(i, SampleCount);
        const float3 Hl = ImportanceSampleGGX(Xi, Roughness);
        const float3 H  = T * Hl.x + B * Hl.y + N * Hl.z;
        const float3 L  = 2.0 * dot(N, H) * H - N;  // V = N

        const float NdotL = dot(N, L);
        if (NdotL > 0.0) {
            // With V = N, VdotH == NdotH, so pdf = D * NdotH / (4 * VdotH) = D / 4.
            const float NdotH    = saturate(dot(N, H));
            const float Pdf      = DistributionGGX(NdotH, Roughness) * 0.25;
            const float SaSample = 1.0 / (float(SampleCount) * Pdf + 0.0001);
            const float Lod      = max(0.5 * log2(SaSample / SaTexel) + 1.0, 0.0);

            Color  += SourceMap.SampleLevel(SourceSampler, DirToEquirectUV(L), Lod).rgb * NdotL;
            Weight += NdotL;
        }
    }

    return float4(Color / max(Weight, 0.0001), 1.0);
}
