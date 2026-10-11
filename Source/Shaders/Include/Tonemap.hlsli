// The final color transform the post-process composite pass applies (see
// PostProcessComposite.hlsl / MeshRenderer's PostProcess) - exposure has
// already been multiplied in by the caller. Every other shader that
// contributes to the scene (PBR.hlsl, Sky.hlsl) writes plain linear HDR
// color with no tonemap or gamma of its own; this is the one place both get
// mapped down together, so an object and the sky behind it always agree.

#ifndef XEN_TONEMAP_HLSLI
#define XEN_TONEMAP_HLSLI

// Narkowicz's fit of the ACES filmic reference curve - the same one Unreal
// and Unity default to. Rolls off highlights instead of clipping them, which
// is what makes a bloomed highlight look like it's glowing instead of
// flat-topped.
float3 ACESFilm(float3 x) {
    const float A = 2.51;
    const float B = 0.03;
    const float C = 2.43;
    const float D = 0.59;
    const float E = 0.14;
    return saturate((x * (A * x + B)) / (x * (C * x + D) + E));
}

float3 TonemapAndEncode(float3 Color) {
    Color = ACESFilm(Color);
    return pow(Color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
}

#endif  // XEN_TONEMAP_HLSLI
