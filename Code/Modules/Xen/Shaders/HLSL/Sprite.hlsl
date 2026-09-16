//
// Created by Jake Rieger on 9/15/2026.
//
// Ported 1:1 from the old GLSL Sprite.vert/Sprite.frag. Vertex attributes are
// matched to the pipeline's VertexLayout by (TEXCOORD, Location) rather than a
// named semantic, mirroring the GLSL side's `layout(location = N)` scheme, so
// the D3D12 backend can build the input layout generically from any
// RHI::VertexLayout without knowing shader-specific semantic names.

// row_major: matches DirectXMath's XMStoreFloat4x4 memory layout exactly, so
// no CPU-side transpose is needed before uploading the constant buffer.
cbuffer FrameData : register(b0) {
    row_major float4x4 ViewProjection;
};

Texture2D Albedo : register(t0);
SamplerState AlbedoSampler : register(s0);

struct VSInput {
    float2 Center   : TEXCOORD0;  // world units
    float2 Size     : TEXCOORD1;  // world units, signed - negative flips
    float Rotation  : TEXCOORD2;  // radians
    float4 UVRect   : TEXCOORD3;  // xy = min uv, zw = uv size
    float4 Tint     : TEXCOORD4;
    uint VertexID   : SV_VertexID;
};

struct PSInput {
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
    float4 Tint : COLOR0;
};

// Triangle strip order: BL, BR, TL, TR.
static const float2 Corners[4] = {
  float2(-0.5, -0.5), float2(0.5, -0.5), float2(-0.5, 0.5), float2(0.5, 0.5)
};

// v flipped against the corner y, because stb_image loads top-down while
// world space is y-up. Sprite top must sample the first row of the image.
static const float2 CornerUVs[4] = {
  float2(0.0, 1.0), float2(1.0, 1.0), float2(0.0, 0.0), float2(1.0, 0.0)
};

PSInput VSMain(VSInput In) {
    const float2 Local = Corners[In.VertexID] * In.Size;

    const float C = cos(In.Rotation);
    const float S = sin(In.Rotation);
    const float2 Rotated = float2(Local.x * C - Local.y * S, Local.x * S + Local.y * C);

    PSInput Out;
    Out.UV   = In.UVRect.xy + CornerUVs[In.VertexID] * In.UVRect.zw;
    Out.Tint = In.Tint;

    const float2 WorldPos = In.Center + Rotated;
    Out.Position          = mul(float4(WorldPos, 0.0, 1.0), ViewProjection);

    return Out;
}

float4 PSMain(PSInput In) : SV_Target {
    const float4 Color = Albedo.Sample(AlbedoSampler, In.UV) * In.Tint;
    clip(Color.a <= 0.0 ? -1.0 : 1.0);
    return Color;
}
