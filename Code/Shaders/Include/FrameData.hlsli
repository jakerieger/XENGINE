// Per-frame constants shared by every mesh-rendering shader (PBR.hlsl,
// Sky.hlsl): declared once here so the C++ mirror in MeshRenderer.cpp
// (FrameConstants) has exactly one HLSL layout to match. Bound once per frame
// at XEN_FRAME_REGISTER (see MaterialBindings.hlsli).

#ifndef XEN_FRAMEDATA_HLSLI
#define XEN_FRAMEDATA_HLSLI

#include "MaterialBindings.hlsli"

cbuffer FrameData : register(XEN_FRAME_REGISTER) {
    row_major float4x4 ViewProjection;
    row_major float4x4 InvViewProjection;  // for reconstructing a world-space view ray per pixel (Sky.hlsl)
    float4 CameraPositionAndPad;           // xyz = CameraPosition
    float4 LightDirectionAndPad;           // xyz = LightDirection (points FROM the light TOWARD the surface)
    float4 LightColorAndIntensity;         // xyz = LightColor, w = LightIntensity
};

#endif  // XEN_FRAMEDATA_HLSLI
