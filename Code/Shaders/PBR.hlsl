// Metallic-roughness Cook-Torrance PBR: one directional light plus image-based
// lighting from an equirectangular environment map and a baked BRDF LUT (see
// BRDFIntegrate.hlsl), no shadows yet. Binding slots follow
// Include/MaterialBindings.hlsli's standardized scheme - see that file for
// what each register means and why.
//
// Loaded as a precompiled DXIL asset out of a pak (see MeshRenderer.cpp),
// never runtime-compiled HLSL from a game's own Content directory.

#include "Include/Common.hlsli"
#include "Include/MaterialBindings.hlsli"

cbuffer FrameData : register(XEN_FRAME_REGISTER) {
    row_major float4x4 ViewProjection;
    float4 CameraPositionAndPad;    // xyz = CameraPosition
    float4 LightDirectionAndPad;    // xyz = LightDirection (points FROM the light TOWARD the surface)
    float4 LightColorAndIntensity;  // xyz = LightColor, w = LightIntensity
};

cbuffer ObjectData : register(XEN_OBJECT_REGISTER) {
    row_major float4x4 Model;
};

cbuffer MaterialData : register(XEN_MATERIAL_REGISTER) {
    float4 AlbedoAndMetallic;   // xyz = Albedo, w = Metallic
    float4 RoughnessAOAndPad;   // x = Roughness, y = AmbientOcclusion
    float4 EmissiveAndPad;      // xyz = Emissive
};

Texture2D AlbedoMap : register(XEN_ALBEDO_TEX_REGISTER);
SamplerState AlbedoSampler : register(XEN_ALBEDO_SAMPLER_REGISTER);

Texture2D NormalMap : register(XEN_NORMAL_TEX_REGISTER);
SamplerState NormalSampler : register(XEN_NORMAL_SAMPLER_REGISTER);

Texture2D MetallicRoughnessMap : register(XEN_METALROUGH_TEX_REGISTER);
SamplerState MetallicRoughnessSampler : register(XEN_METALROUGH_SAMPLER_REGISTER);

Texture2D AmbientOcclusionMap : register(XEN_AO_TEX_REGISTER);
SamplerState AmbientOcclusionSampler : register(XEN_AO_SAMPLER_REGISTER);

Texture2D EmissiveMap : register(XEN_EMISSIVE_TEX_REGISTER);
SamplerState EmissiveSampler : register(XEN_EMISSIVE_SAMPLER_REGISTER);

// Scene-level (bound once per frame, not per material) - see
// MaterialBindings.hlsli.
Texture2D EnvironmentMap : register(XEN_ENVIRONMENT_TEX_REGISTER);  // prefiltered specular: mip = roughness
SamplerState EnvironmentSampler : register(XEN_ENVIRONMENT_SAMPLER_REGISTER);

Texture2D IrradianceMap : register(XEN_IRRADIANCE_TEX_REGISTER);  // cosine-convolved diffuse lighting
SamplerState IrradianceSampler : register(XEN_IRRADIANCE_SAMPLER_REGISTER);

Texture2D BrdfLUT : register(XEN_BRDF_LUT_TEX_REGISTER);
SamplerState BrdfLUTSampler : register(XEN_BRDF_LUT_SAMPLER_REGISTER);

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
    float3 WorldTangent  : TEXCOORD2;
    float2 UV            : TEXCOORD3;
};

PSInput VSMain(VSInput In) {
    PSInput Out;

    const float4 WorldPos = mul(float4(In.Position, 1.0), Model);
    Out.WorldPosition     = WorldPos.xyz;
    // (float3x3)Model transforms normals/tangents correctly only under
    // uniform scale - non-uniform scale needs the inverse-transpose instead.
    // Fine for now; worth revisiting once meshes are scaled non-uniformly.
    Out.WorldNormal = normalize(mul(In.Normal, (float3x3)Model));
    // Deliberately NOT normalized here: In.Tangent is (0,0,0) for a mesh
    // with no authored tangent data (see ApplyNormalMap's comment), and
    // normalizing a zero vector is NaN - ApplyNormalMap's zero-length check
    // needs to see the real (possibly zero) vector, not an already-NaN one.
    Out.WorldTangent = mul(In.Tangent, (float3x3)Model);
    Out.UV           = In.UV;
    Out.Position     = mul(WorldPos, ViewProjection);

    return Out;
}


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

// Fresnel-Schlick with a roughness term folded in (Sebastien Lagarde's
// "Moving Frostbite to PBR" formulation) - the direct-lighting FresnelSchlick
// above over-brightens grazing angles on rough indirect surfaces without it.
float3 FresnelSchlickRoughness(float CosTheta, float3 F0, float Roughness) {
    return F0 + (max(float3(1.0 - Roughness, 1.0 - Roughness, 1.0 - Roughness), F0) - F0) *
                pow(clamp(1.0 - CosTheta, 0.0, 1.0), 5.0);
}

// Tangent-space normal (sampled from NormalMap, so callers always pass a
// texture - MeshRenderer binds a flat (0.5, 0.5, 1.0) placeholder when a
// material has none, decoding to (0, 0, 1) i.e. "no perturbation") to world
// space, via a Gram-Schmidt re-orthogonalized TBN basis. Bitangent is
// derived as cross(N, T) rather than read from a stored handedness sign -
// MeshVertex has no fourth tangent component (see MeshCache.hpp) - so this
// comes out flipped on mirrored UV islands. Acceptable for now; fixing it
// means carrying glTF's tangent.w through the whole mesh pipeline.
float3 ApplyNormalMap(float3 TangentSpaceNormal, float3 WorldNormal, float3 WorldTangent) {
    const float3 N = normalize(WorldNormal);

    // glTF's TANGENT attribute is optional, and most DCC exporters omit it
    // unless the mesh is actually set up for normal mapping (Blender's glTF
    // exporter does) - MeshCache zero-fills Tangent for a mesh with none
    // (see MeshCache.cpp), which arrives here as WorldTangent == 0.
    // normalize()-ing that is NaN, which poisons every downstream lighting
    // term for the whole mesh, not just the normal - fall back to an
    // arbitrary-but-valid tangent basis instead. That's exactly correct for
    // the overwhelmingly common case this is actually hit in (no normal map
    // assigned, so TangentSpaceNormal is the flat placeholder and any valid
    // orthonormal basis reproduces N unchanged); only wrong - not NaN - if a
    // real normal map is ever assigned to a mesh with no real tangents.
    float3 T;
    if (dot(WorldTangent, WorldTangent) < 1e-8) {
        const float3 Up = abs(N.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
        T = normalize(cross(Up, N));
    } else {
        T = normalize(WorldTangent - N * dot(WorldTangent, N));
    }

    const float3 B = cross(N, T);
    const float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(TangentSpaceNormal, TBN));
}

float4 PSMain(PSInput In) : SV_Target {
    // Every texture sample MULTIPLIES its matching constant factor (never
    // replaces it) - see MaterialBindings.hlsli. A material with no map
    // assigned is bound to a white (or flat-normal) placeholder by
    // MeshRenderer, so this is branch-free either way.
    const float3 Albedo   = AlbedoAndMetallic.xyz * AlbedoMap.Sample(AlbedoSampler, In.UV).rgb;
    const float4 MR       = MetallicRoughnessMap.Sample(MetallicRoughnessSampler, In.UV);
    const float Metallic  = AlbedoAndMetallic.w * MR.b;
    const float Roughness = max(RoughnessAOAndPad.x * MR.g, 0.045);
    const float AO        = RoughnessAOAndPad.y * AmbientOcclusionMap.Sample(AmbientOcclusionSampler, In.UV).r;
    const float3 Emissive = EmissiveAndPad.xyz * EmissiveMap.Sample(EmissiveSampler, In.UV).rgb;

    const float3 TangentSpaceNormal = NormalMap.Sample(NormalSampler, In.UV).xyz * 2.0 - 1.0;
    const float3 N = ApplyNormalMap(TangentSpaceNormal, In.WorldNormal, In.WorldTangent);

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

    // Image-based lighting, split-sum style: the environment maps supply the
    // incoming light, BrdfLUT supplies how much of it this material reflects
    // at this view angle and roughness (F0 * scale + bias).
    //
    // EnvironmentMap is the GGX-prefiltered specular map (EnvironmentBaker),
    // one roughness level per mip, so roughness picks a mip: blurry
    // reflections cost one sample. IrradianceMap is the cosine-convolved
    // diffuse lighting, already integrated over the hemisphere. Both use
    // SampleLevel - implicit-derivative Sample would put a visible seam where
    // atan2 wraps in DirToEquirectUV. A scene with no environment binds a
    // 1x2 placeholder sky for both (one mip, so the LOD below clamps to 0).
    const float NdotV = max(dot(N, V), 0.0);
    const float3 FIndirect = FresnelSchlickRoughness(NdotV, F0, Roughness);
    const float3 KDiffuseIndirect = (1.0 - FIndirect) * (1.0 - Metallic);

    const float3 Irradiance = IrradianceMap.SampleLevel(IrradianceSampler, DirToEquirectUV(N), 0).rgb;
    const float3 DiffuseIBL = Irradiance * Albedo * KDiffuseIndirect * AO;

    uint EnvWidth, EnvHeight, EnvLevels;
    EnvironmentMap.GetDimensions(0, EnvWidth, EnvHeight, EnvLevels);
    const float EnvLod = Roughness * float(EnvLevels - 1);

    const float3 R = reflect(-V, N);
    const float3 PrefilteredColor = EnvironmentMap.SampleLevel(EnvironmentSampler, DirToEquirectUV(R), EnvLod).rgb;
    const float2 EnvBRDF = BrdfLUT.SampleLevel(BrdfLUTSampler, float2(NdotV, Roughness), 0).rg;
    const float3 SpecularIBL = PrefilteredColor * (F0 * EnvBRDF.x + EnvBRDF.y) * AO;

    const float3 Ambient = DiffuseIBL + SpecularIBL;

    float3 Color = Ambient + DirectLight + Emissive;

    // Reinhard tonemap + gamma correction: the swap chain is a plain UNORM
    // target (no sRGB view, no HDR/tonemap pass yet), so this has to happen
    // here or values above 1.0 just clip.
    Color = Color / (Color + float3(1.0, 1.0, 1.0));
    Color = pow(Color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));

    return float4(Color, 1.0);
}
