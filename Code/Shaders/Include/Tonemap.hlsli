// The final color transform every shader writing to the (plain UNORM) color
// target applies, so an object and the sky behind it agree on exposure and
// gamma: the swap chain has no sRGB view and there's no HDR/tonemap pass yet,
// so this has to happen in the pixel shader or values above 1.0 just clip.

#ifndef XEN_TONEMAP_HLSLI
#define XEN_TONEMAP_HLSLI

// Reinhard tonemap + gamma encode.
float3 TonemapAndEncode(float3 Color) {
    Color = Color / (Color + float3(1.0, 1.0, 1.0));
    return pow(Color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
}

#endif  // XEN_TONEMAP_HLSLI
