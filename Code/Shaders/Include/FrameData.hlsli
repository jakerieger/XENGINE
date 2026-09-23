// Per-frame constants shared by every mesh-rendering shader (PBR.hlsl,
// Sky.hlsl): declared once here so the C++ mirror in MeshRenderer.cpp
// (FrameConstants) has exactly one HLSL layout to match. Bound once per frame
// at XEN_FRAME_REGISTER (see MaterialBindings.hlsli).

#ifndef XEN_FRAMEDATA_HLSLI
#define XEN_FRAMEDATA_HLSLI

#include "MaterialBindings.hlsli"

cbuffer FrameData : register(XEN_FRAME_REGISTER) {
    row_major float4x4 ViewProjection;     // JITTERED (see TAA.hpp) - what every pass actually rasterizes with
    row_major float4x4 InvViewProjection;  // inverse of the JITTERED matrix above - Sky's world-space view ray

    // TAA motion vectors (PBR.hlsl/Sky.hlsl's PSOutput.Velocity) need clip
    // positions WITHOUT jitter - the sub-pixel wobble that makes TAA work
    // must not itself register as motion, or every pixel would "move" a
    // little every frame even when nothing actually does.
    row_major float4x4 UnjitteredViewProjection;     // this frame, no jitter - the "current" clip position
    row_major float4x4 InvUnjitteredViewProjection;  // inverse of the above - Sky's motion-vector ray reconstruction
    row_major float4x4 PrevViewProjection;           // last frame, no jitter - the "previous" clip position

    float4 CameraPositionAndPad;           // xyz = CameraPosition
    float4 LightDirectionAndPad;           // xyz = LightDirection (points FROM the light TOWARD the surface)
    float4 LightColorAndIntensity;         // xyz = LightColor, w = LightIntensity

    // Directional-light shadow map (see MeshRenderer's shadow pass). During
    // that pass ViewProjection above IS the light's, so Shadow.hlsl needs
    // nothing more; the fields below are what PBR.hlsl reads to look a
    // surface up in the finished map.
    row_major float4x4 LightViewProjection;
    float4 ShadowParams;                   // x = 1 when a shadow map is bound (0 = everything is lit),
                                           // y = constant depth bias (light NDC z), z = normal offset (world units),
                                           // w = PCF radius in texels
    float4 ShadowParams2;                  // x = 1 / shadow map size, y = shadow distance, z = fade-out length

    float4 InvScreenSizeAndPad;  // xy = 1 / render target size in pixels - PBR.hlsl's own SSAO screen UV (SV_Position.xy * this)
};

#endif  // XEN_FRAMEDATA_HLSLI
