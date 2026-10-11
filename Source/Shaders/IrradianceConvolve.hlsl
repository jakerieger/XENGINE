// Bakes one face of the diffuse irradiance cube map: each output texel is the
// source environment convolved with a cosine lobe around that texel's
// direction, so a diffuse surface reads its whole-hemisphere lighting with one
// cube sample instead of integrating it per pixel. Drawn once per face by
// EnvironmentBaker into a small (32x32 per face) cube - irradiance is so
// low-frequency that more resolution would just be wasted memory.
//
// Cosine-weighted sampling makes the average of the sampled radiance equal to
// E / PI (irradiance over pi), which is exactly what PBR.hlsl multiplies by
// albedo - a uniform environment of radiance L bakes to L everywhere. Source
// samples are taken from the equirect source's mip pyramid at a level chosen
// from each sample's pdf, so a bright small feature (a sun disc) is spread
// smoothly instead of showing up as speckle.

#include "Include/Common.hlsli"
#include "Include/Fullscreen.hlsli"

cbuffer BakeParams : register(b0) {
    float4 Params;   // y = sample count, w = face
    float4 Params2;  // unused (shared layout with PrefilterEnvironment.hlsl)
};

Texture2D SourceMap : register(t0);
SamplerState SourceSampler : register(s0);

VSOutput VSMain(uint VertexID : SV_VertexID) {
    return FullscreenVertex(VertexID, 0.0);
}

float4 PSMain(VSOutput In) : SV_Target {
    uint SourceWidth, SourceHeight, SourceLevels;
    SourceMap.GetDimensions(0, SourceWidth, SourceHeight, SourceLevels);

    const uint SampleCount = (uint) Params.y;
    const uint Face        = (uint) Params.w;
    const float3 N         = CubeFaceUVToDir(Face, In.UV);

    const float3 Up = abs(N.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    const float3 T  = normalize(cross(Up, N));
    const float3 B  = cross(N, T);

    float3 Sum = float3(0.0, 0.0, 0.0);

    for (uint i = 0; i < SampleCount; ++i) {
        const float2 Xi = Hammersley(i, SampleCount);

        // Cosine-weighted hemisphere sample around N: pdf = cos(theta) / PI.
        const float Phi      = 2.0 * PI * Xi.x;
        const float CosTheta = sqrt(1.0 - Xi.y);
        const float SinTheta = sqrt(Xi.y);
        const float3 L       = T * (cos(Phi) * SinTheta) + B * (sin(Phi) * SinTheta) + N * CosTheta;

        const float Pdf      = max(CosTheta, 0.0001) / PI;
        const float SaSample = 1.0 / (float(SampleCount) * Pdf);
        const float SaTexel  = EquirectTexelSolidAngle(L, float(SourceWidth), float(SourceHeight));
        const float Lod      = max(0.5 * log2(SaSample / SaTexel) + 1.0, 0.0);

        Sum += SourceMap.SampleLevel(SourceSampler, DirToEquirectUV(L), Lod).rgb;
    }

    return float4(Sum / float(SampleCount), 1.0);
}
