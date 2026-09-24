// Every point/spot light in the scene, collected and uploaded by
// MeshRenderer once per frame (see Code/Modules/Xen/LightData.hpp's
// LightConstants, which this must match exactly) - a plain CPU-built,
// capped array. Forward+ tile culling (Code/Shaders/LightCulling.hlsl,
// Include/LightCulling.hlsli) sits on top of this unchanged: it only adds a
// per-tile index list into this same array, so PBR.hlsl's pixel shader
// loops over a handful of indices instead of [0, LightCount) - the array
// format itself never needed to change for that.
// MAX_LIGHTS bounds a normal constant buffer's own size limit (65536 bytes -
// comfortably enough for a hand-authored scene; a scene with more active
// lights than this needs a different packing, not just more tiling).

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
