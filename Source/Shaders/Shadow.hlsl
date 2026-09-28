// Depth-only pass for the directional light's shadow map: every shadow caster,
// rendered from the light. There is no pixel stage at all - the depth buffer is
// the output. MeshRenderer binds the light's view-projection where the camera's
// normally goes (FrameData's ViewProjection), so this shares the frame and
// object constant buffers with PBR.hlsl.
//
// Loaded as a precompiled DXIL asset out of a pak (see MeshRenderer.cpp).

#include "Include/FrameData.hlsli"

cbuffer ObjectData : register(XEN_OBJECT_REGISTER) {
    row_major float4x4 Model;
};

struct VSInput {
    float3 Position : TEXCOORD0;
};

float4 VSMain(VSInput In) : SV_Position {
    return mul(mul(float4(In.Position, 1.0), Model), ViewProjection);
}
