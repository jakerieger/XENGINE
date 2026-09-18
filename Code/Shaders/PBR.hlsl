// Metallic-roughness Cook-Torrance PBR, one directional light, no IBL/shadows
// yet. Constant-factor material (no textures) - MeshRenderer's b2 register.
//
// Loaded as an asset (Content/shaders/pbr.hlsl -> ASSET("shaders/pbr.hlsl")),
// not embedded in C++ like SpriteRenderer's shader still is - proving the
// shader system actually goes through AssetRegistry like every other asset
// kind, per the plan discussed with the engine's author.

cbuffer FrameData : register(b0) {
    row_major float4x4 ViewProjection;
    float4 CameraPositionAndPad;    // xyz = CameraPosition
    float4 LightDirectionAndPad;    // xyz = LightDirection (points FROM the light TOWARD the surface)
    float4 LightColorAndIntensity;  // xyz = LightColor, w = LightIntensity
};

cbuffer ObjectData : register(b1) {
    row_major float4x4 Model;
};

cbuffer MaterialData : register(b2) {
    float4 AlbedoAndMetallic;   // xyz = Albedo, w = Metallic
    float4 RoughnessAOAndPad;   // x = Roughness, y = AmbientOcclusion
    float4 EmissiveAndPad;      // xyz = Emissive
};

struct VSInput {
    float3 Position : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float3 Tangent  : TEXCOORD2;
    float2 UV       : TEXCOORD3;
};

struct PSInput {
    float4 Position      : SV_Position;
    float3 WorldPosition : TEXCOORD0;
    float3 WorldNormal   : TEXCOORD1;
    float2 UV            : TEXCOORD2;
};

PSInput VSMain(VSInput In) {
    PSInput Out;

    const float4 WorldPos = mul(float4(In.Position, 1.0), Model);
    Out.WorldPosition     = WorldPos.xyz;
    // (float3x3)Model transforms normals correctly only under uniform scale -
    // non-uniform scale needs the inverse-transpose instead. Fine for now;
    // worth revisiting once meshes are scaled non-uniformly.
    Out.WorldNormal = normalize(mul(In.Normal, (float3x3)Model));
    Out.UV          = In.UV;
    Out.Position    = mul(WorldPos, ViewProjection);

    return Out;
}

static const float PI = 3.14159265359;

float DistributionGGX(float3 N, float3 H, float Roughness) {
    const float A = Roughness * Roughness;
    const float A2 = A * A;
    const float NdotH = max(dot(N, H), 0.0);
    const float NdotH2 = NdotH * NdotH;

    float Denom = (NdotH2 * (A2 - 1.0) + 1.0);
    Denom = PI * Denom * Denom;

    return A2 / max(Denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float Roughness) {
    const float R = Roughness + 1.0;
    const float K = (R * R) / 8.0;
    return NdotV / max(NdotV * (1.0 - K) + K, 0.0000001);
}

float GeometrySmith(float3 N, float3 V, float3 L, float Roughness) {
    const float NdotV = max(dot(N, V), 0.0);
    const float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, Roughness) * GeometrySchlickGGX(NdotL, Roughness);
}

float3 FresnelSchlick(float CosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - CosTheta, 0.0, 1.0), 5.0);
}

float4 PSMain(PSInput In) : SV_Target {
    const float3 Albedo    = AlbedoAndMetallic.xyz;
    const float Metallic   = AlbedoAndMetallic.w;
    const float Roughness  = RoughnessAOAndPad.x;
    const float AO         = RoughnessAOAndPad.y;
    const float3 Emissive  = EmissiveAndPad.xyz;

    const float3 N = normalize(In.WorldNormal);
    const float3 V = normalize(CameraPositionAndPad.xyz - In.WorldPosition);
    const float3 L = normalize(-LightDirectionAndPad.xyz);
    const float3 H = normalize(V + L);

    // Dielectrics start near 0.04 reflectance at normal incidence; metals
    // tint their reflectance by their own albedo instead.
    const float3 F0 = lerp(float3(0.04, 0.04, 0.04), Albedo, Metallic);

    const float NDF = DistributionGGX(N, H, Roughness);
    const float G   = GeometrySmith(N, V, L, Roughness);
    const float3 F  = FresnelSchlick(max(dot(H, V), 0.0), F0);

    const float3 KSpecular = F;
    // Metals absorb all diffuse light - only dielectrics scatter it.
    const float3 KDiffuse = (1.0 - KSpecular) * (1.0 - Metallic);

    const float3 Numerator   = NDF * G * F;
    const float Denominator  = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    const float3 Specular    = Numerator / Denominator;

    const float NdotL       = max(dot(N, L), 0.0);
    const float3 Radiance   = LightColorAndIntensity.xyz * LightColorAndIntensity.w;
    const float3 DirectLight = (KDiffuse * Albedo / PI + Specular) * Radiance * NdotL;

    // Flat ambient term until IBL exists - a placeholder, not a real
    // indirect-lighting estimate.
    const float3 Ambient = float3(0.03, 0.03, 0.03) * Albedo * AO;

    float3 Color = Ambient + DirectLight + Emissive;

    // Reinhard tonemap + gamma correction: the swap chain is a plain UNORM
    // target (no sRGB view, no HDR/tonemap pass yet), so this has to happen
    // here or values above 1.0 just clip.
    Color = Color / (Color + float3(1.0, 1.0, 1.0));
    Color = pow(Color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));

    return float4(Color, 1.0);
}
