// Every point/spot light in the scene, collected and uploaded by
// MeshRenderer once per frame (see MeshRenderer.cpp's LightConstants, which
// this must match exactly) - a plain CPU-built array, not a culled per-tile
// list (see TAA.hpp-adjacent design notes elsewhere: this engine is a plain
// forward renderer, not (yet) Forward+ - tile-based light culling is a
// deferred follow-on, not something this array format needs to anticipate).
// MAX_LIGHTS bounds a normal constant buffer's own size limit (65536 bytes -
// comfortably enough for a hand-authored scene; a scene with more active
// lights than this needs tiled culling regardless of how they're packed).

#ifndef XEN_LIGHTDATA_HLSLI
#define XEN_LIGHTDATA_HLSLI

#include "MaterialBindings.hlsli"

#define XEN_MAX_LIGHTS 128

#define XEN_LIGHT_TYPE_POINT 0.0
#define XEN_LIGHT_TYPE_SPOT  1.0

struct Light {
    float4 PositionAndRange;   // xyz = world position, w = range (distance at which the light reaches zero)
    float4 ColorAndIntensity;  // xyz = linear color, w = intensity
    float4 DirectionAndType;   // xyz = normalized direction (spot only, unused for point), w = XEN_LIGHT_TYPE_*
    float4 ConeAnglesAndPad;   // x = cos(InnerConeAngle), y = cos(OuterConeAngle) - spot only, unused for point
};

cbuffer LightData : register(XEN_LIGHT_REGISTER) {
    uint LightCount;
    uint3 _LightDataPad;
    Light Lights[XEN_MAX_LIGHTS];
};

#endif  // XEN_LIGHTDATA_HLSLI
