// Bakes one face of one roughness level of the prefiltered specular
// environment cube map: each output texel is the source environment convolved
// with the GGX specular lobe for that texel's direction, so a glossy surface
// can read a blurry reflection with a single cube sample instead of
// integrating the hemisphere per pixel. Drawn once per (mip, face) of the
// destination by EnvironmentBaker; a mip's roughness comes from
// RoughnessForMip (Common.hlsli), the same mapping PBR.hlsl inverts.
//
// Split-sum approximation: N = V = R (the standard assumption that lets one
// texture cover every view angle), so the lobe is symmetric around the
// output direction. Source samples are taken from the equirect source's mip
// pyramid at a level chosen from each sample's pdf (Krivanek & Colbert,
// "Real-time Shading with Filtered Importance Sampling") - without that, a
// very bright small feature like a sun disc turns into speckle at low sample
// counts - using the source texel's solid angle at that sample's own latitude
// (equirect texels shrink toward the poles).

#include "Include/Common.hlsli"
#include "Include/Fullscreen.hlsli"

cbuffer BakeParams : register(b0) {
    float4 Params;   // x = mip, y = sample count, z = mip count, w = face
    float4 Params2;  // x = destination size (texels per face side at this mip)
};

Texture2D SourceMap : register(t0);
SamplerState SourceSampler : register(s0);

VSOutput VSMain(uint VertexID : SV_VertexID) {
    return FullscreenVertex(VertexID, 0.0);
}

float DistributionGGX(float NdotH, float Roughness) {
    const float A     = Roughness * Roughness;
    const float A2    = A * A;
    const float Denom = NdotH * NdotH * (A2 - 1.0) + 1.0;
    return A2 / max(PI * Denom * Denom, 0.0000001);
}

float4 PSMain(VSOutput In) : SV_Target {
    uint SourceWidth, SourceHeight, SourceLevels;
    SourceMap.GetDimensions(0, SourceWidth, SourceHeight, SourceLevels);

    const uint Mip         = (uint) Params.x;
    const uint SampleCount = (uint) Params.y;
    const uint Levels      = (uint) Params.z;
    const uint Face        = (uint) Params.w;

    const float Roughness = RoughnessForMip(Mip, Levels);
    const float3 N        = CubeFaceUVToDir(Face, In.UV);

    // Roughness 0 is a mirror: no lobe to integrate, just the source itself,
    // filtered down to this texel's footprint (the destination can be
    // smaller than the source). A face texel covers about (4*PI/6) / size^2
    // steradians.
    if (Roughness == 0.0) {
        const float SaDest = (4.0 * PI / 6.0) / (Params2.x * Params2.x);
        const float SaSrc  = EquirectTexelSolidAngle(N, float(SourceWidth), float(SourceHeight));
        const float Lod    = max(0.5 * log2(SaDest / SaSrc), 0.0);
        return float4(SourceMap.SampleLevel(SourceSampler, DirToEquirectUV(N), Lod).rgb, 1.0);
    }

    const float3 Up = abs(N.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    const float3 T  = normalize(cross(Up, N));
    const float3 B  = cross(N, T);

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
            const float SaTexel  = EquirectTexelSolidAngle(L, float(SourceWidth), float(SourceHeight));
            const float Lod      = max(0.5 * log2(SaSample / SaTexel) + 1.0, 0.0);

            Color  += SourceMap.SampleLevel(SourceSampler, DirToEquirectUV(L), Lod).rgb * NdotL;
            Weight += NdotL;
        }
    }

    return float4(Color / max(Weight, 0.0001), 1.0);
}
