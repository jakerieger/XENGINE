// Metallic-roughness Cook-Torrance PBR: one directional light plus image-based
// lighting from baked environment cube maps (see EnvironmentBaker) and a baked
// BRDF LUT (see BRDFIntegrate.hlsl), plus a PCF-filtered shadow map for the
// light (see Shadow.hlsl / MeshRenderer's shadow pass). Binding slots follow
// Include/MaterialBindings.hlsli's standardized scheme - see that file for
// what each register means and why.
//
// Loaded as a precompiled DXIL asset out of a pak (see MeshRenderer.cpp),
// never runtime-compiled HLSL from a game's own Content directory.

#include "Include/Common.hlsli"
#include "Include/FrameData.hlsli"
#include "Include/LightData.hlsli"
#include "Include/LightCulling.hlsli"
#include "Include/MaterialBindings.hlsli"

cbuffer ObjectData : register(XEN_OBJECT_REGISTER) {
    row_major float4x4 Model;
    row_major float4x4 PrevModel;  // last frame's Model - TAA motion vectors for a moving/rotating actor (see PSOutput)
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

// Separate single-channel maps (.r), not glTF's packed G/B texture - either
// may be unassigned (bound to the white placeholder) independently of the
// other. See MaterialBindings.hlsli.
Texture2D RoughnessMap : register(XEN_ROUGHNESS_TEX_REGISTER);
SamplerState RoughnessSampler : register(XEN_ROUGHNESS_SAMPLER_REGISTER);

Texture2D MetallicMap : register(XEN_METALLIC_TEX_REGISTER);
SamplerState MetallicSampler : register(XEN_METALLIC_SAMPLER_REGISTER);

Texture2D AmbientOcclusionMap : register(XEN_AO_TEX_REGISTER);
SamplerState AmbientOcclusionSampler : register(XEN_AO_SAMPLER_REGISTER);

Texture2D EmissiveMap : register(XEN_EMISSIVE_TEX_REGISTER);
SamplerState EmissiveSampler : register(XEN_EMISSIVE_SAMPLER_REGISTER);

// Scene-level (bound once per frame, not per material) - see
// MaterialBindings.hlsli.
TextureCube EnvironmentMap : register(XEN_ENVIRONMENT_TEX_REGISTER);  // prefiltered specular: mip = roughness (RoughnessForMip)
SamplerState EnvironmentSampler : register(XEN_ENVIRONMENT_SAMPLER_REGISTER);

TextureCube IrradianceMap : register(XEN_IRRADIANCE_TEX_REGISTER);  // cosine-convolved diffuse lighting
SamplerState IrradianceSampler : register(XEN_IRRADIANCE_SAMPLER_REGISTER);

Texture2D BrdfLUT : register(XEN_BRDF_LUT_TEX_REGISTER);
SamplerState BrdfLUTSampler : register(XEN_BRDF_LUT_SAMPLER_REGISTER);

Texture2D<float> ShadowMap : register(XEN_SHADOW_MAP_TEX_REGISTER);
SamplerComparisonState ShadowSampler : register(XEN_SHADOW_MAP_SAMPLER_REGISTER);

// Screen-space ambient occlusion (see SSAO.hpp) - sampled by screen position,
// not UV, since it's a full-screen texture unrelated to this surface's own
// texture coordinates.
Texture2D<float> SSAOMap : register(XEN_SSAO_TEX_REGISTER);
SamplerState SSAOSampler : register(XEN_SSAO_SAMPLER_REGISTER);

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
    // TAA motion vectors (see PSOutput below) - both UNJITTERED, so the
    // per-frame sub-pixel jitter that makes TAA work doesn't itself read as
    // motion. PrevClip uses PrevModel, not Model, so a moving or rotating
    // actor gets a correct velocity too, not just camera motion.
    float4 CurrClip       : TEXCOORD4;
    float4 PrevClip       : TEXCOORD5;
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
    Out.Position     = mul(WorldPos, ViewProjection);  // JITTERED - the pixel grid this actually rasterizes to

    Out.CurrClip = mul(WorldPos, UnjitteredViewProjection);
    const float4 PrevWorldPos = mul(float4(In.Position, 1.0), PrevModel);
    Out.PrevClip = mul(PrevWorldPos, PrevViewProjection);

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

// Cook-Torrance direct lighting for one already-attenuated Radiance arriving
// from direction L - shared by the directional light (Radiance includes its
// shadow term) and every point/spot light below (Radiance includes distance/
// cone falloff instead). Pulled out once rather than duplicated per light
// type, since the BRDF evaluation itself doesn't care where Radiance/L came
// from.
float3 EvaluateDirectLighting(float3 N, float3 V, float3 L, float3 Albedo, float Metallic, float Roughness,
                              float3 F0, float3 Radiance) {
    const float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return float3(0.0, 0.0, 0.0);

    const float3 H = normalize(V + L);
    const float NDF = DistributionGGX(N, H, Roughness);
    const float G   = GeometrySmith(N, V, L, Roughness);
    const float3 F  = FresnelSchlick(max(dot(H, V), 0.0), F0);

    const float3 KSpecular = F;
    // Metals absorb all diffuse light - only dielectrics scatter it.
    const float3 KDiffuse = (1.0 - KSpecular) * (1.0 - Metallic);

    const float3 Specular = (NDF * G * F) / max(4.0 * max(dot(N, V), 0.0) * NdotL, 0.0001);
    return (KDiffuse * Albedo / PI + Specular) * Radiance * NdotL;
}

// Karis's windowed inverse-square falloff ("Real Shading in Unreal Engine 4",
// SIGGRAPH 2013) - physically-plausible 1/d^2 close to the light, smoothly
// windowed to exactly zero at Range instead of a hard if-cutoff, which would
// pop visibly as a light moves or a receiver crosses the boundary.
float DistanceAttenuation(float Distance, float Range) {
    const float DistanceOverRange = Distance / max(Range, 0.0001);
    const float Window = saturate(1.0 - pow(DistanceOverRange, 4.0));
    return (Window * Window) / max(Distance * Distance, 0.0001);
}

// Smooth angular falloff between a spot light's inner and outer cone - fully
// lit inside CosInner, zero outside CosOuter, squared for a softer-edged
// transition than a linear ramp (matches the same "smoothstep-ish" shaping
// DistanceAttenuation's own squared window uses).
float SpotAttenuation(float3 ToLightDir, float3 SpotForward, float CosInner, float CosOuter) {
    // ToLightDir points FROM the surface TO the light; SpotForward is the
    // direction the light itself points (the light's own -Z, see
    // SpotLightComponent::GetDirection) - the angle between them is measured
    // against the light shining TOWARD the surface, hence the negation.
    const float CosAngle = dot(-ToLightDir, SpotForward);
    const float Falloff  = saturate((CosAngle - CosOuter) / max(CosInner - CosOuter, 0.0001));
    return Falloff * Falloff;
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

// Jorge Jimenez's interleaved gradient noise ("Next Generation Post
// Processing in Call of Duty: Advanced Warfare") - a cheap, no-texture
// per-pixel value in [0, 1) with no visible large-scale structure of its
// own, unlike a low-frequency hash. Used below to rotate the PCF kernel a
// different amount per screen pixel.
float InterleavedGradientNoise(float2 ScreenPos) {
    const float3 Magic = float3(0.06711056, 0.00583715, 52.9829189);
    return frac(Magic.z * frac(dot(ScreenPos, Magic.xy)));
}

// Fraction of the directional light reaching this point: 1 = fully lit,
// 0 = fully shadowed. The comparison sampler does the depth test and a 2x2
// bilinear blend per tap; the 5x5 taps around it (spaced ShadowParams.w
// texels apart) widen that into a soft edge. 5x5 (25 taps), not the more
// usual 3x3 (9): 9 discrete comparison outcomes averaged together is only a
// 1/9-of-full-range staircase, which on an otherwise-smooth flat receiver
// (a floor, not a curved caster) is fine detail enough to read as grainy
// "acne" once anything darkens the shadow enough to actually look at it
// closely - which DirectionalLightComponent::ShadowAmbientDarkening does on
// purpose. 25 steps reads as a smooth gradient at the same cost this map
// already affords (one small, dedicated depth target).
//
// The 5x5 grid is also rotated by a per-pixel angle (ScreenPos, via
// InterleavedGradientNoise) rather than kept axis-aligned: a large flat
// receiver viewed at a grazing/receding angle massively minifies the shadow
// map (many texels per screen pixel, especially toward the horizon), and
// this map has no mip chain to prefilter that the way a color texture would
// (see MipGenerator) - sampling a small, FIXED, axis-aligned tap pattern
// against that undersampled, regular texel grid is classic minification
// aliasing, and on a regular grid that shows up as moire: coherent rippled
// "rings" across the whole receiver, not random noise, and not localized to
// any caster's shadow. Rotating the kernel per pixel doesn't add
// information - it's still only 25 taps - but it turns that coherent,
// eye-catching interference pattern into far-less-objectionable per-pixel
// grain, the standard mitigation for this class of problem (and exactly
// what would let a future TAA pass average away cleanly, unlike a fixed
// grid). Properly solving the underlying aliasing - a receiver-adaptive
// kernel radius, or real shadow-map filtering - is part of the broader
// anti-aliasing work, not this fix.
//
// Acne is fought at the receiver, in two ways: the lookup point is pushed out
// along the geometric normal (more for surfaces at a grazing angle to the
// light, where a texel's depth varies most), and the compared depth is pulled
// slightly toward the light. Both are sized in shadow-map texels by
// MeshRenderer, so they stay right as the map's coverage or size changes.
float ShadowVisibility(float3 WorldPosition, float3 GeometricNormal, float NdotL, float2 ScreenPos) {
    if (ShadowParams.x < 0.5 || NdotL <= 0.0) return 1.0;

    const float SinTheta = sqrt(saturate(1.0 - NdotL * NdotL));
    const float3 Offset = GeometricNormal * ShadowParams.z * SinTheta;

    const float4 LightClip = mul(float4(WorldPosition + Offset, 1.0), LightViewProjection);
    const float3 Ndc = LightClip.xyz / LightClip.w;
    const float2 UV = float2(Ndc.x * 0.5 + 0.5, 0.5 - Ndc.y * 0.5);

    // Outside the map's footprint (or past its far plane) nothing is known to
    // occlude the point, so it stays lit.
    if (any(UV < 0.0) || any(UV > 1.0) || Ndc.z > 1.0) return 1.0;

    const float Depth = Ndc.z - ShadowParams.y;

    // How much shadow-map UV this one screen pixel actually covers - large
    // and grazing/distant on a big flat receiver (the far end of a ground
    // plane), tiny dead ahead near the camera. A fixed tap spacing (just
    // ShadowParams.w texels) undersamples the map wherever this exceeds it:
    // neighboring screen pixels then land on wildly different combinations
    // of the same handful of underlying texels, which is exactly the
    // coherent large-wavelength ripple this fixes (confirmed by rotating the
    // kernel - see InterleavedGradientNoise below - making no visible
    // difference to it: that only redistributes samples *within* a fixed
    // small neighborhood, and the aliasing here comes from the neighborhood
    // itself being too small, not from which few texels inside it get read).
    // Never smaller than the plain texel-spaced kernel, so this only ever
    // widens it, never sharpens a well-sampled area.
    const float2 PixelFootprint = fwidth(UV);
    const float2 Step = max(ShadowParams2.x * ShadowParams.w, PixelFootprint * 0.5);

    const float Angle = InterleavedGradientNoise(ScreenPos) * (2.0 * PI);
    float SinA, CosA;
    sincos(Angle, SinA, CosA);

    float Sum = 0.0;
    [unroll] for (int Y = -2; Y <= 2; ++Y) {
        [unroll] for (int X = -2; X <= 2; ++X) {
            const float2 Tap = float2(X, Y);
            const float2 Rotated = float2(Tap.x * CosA - Tap.y * SinA, Tap.x * SinA + Tap.y * CosA);
            Sum += ShadowMap.SampleCmpLevelZero(ShadowSampler, UV + Rotated * Step, Depth);
        }
    }
    const float Visibility = Sum / 25.0;

    // Fade out over the last stretch of the shadow distance rather than
    // cutting off at the edge of the map.
    const float Distance = length(CameraPositionAndPad.xyz - WorldPosition);
    const float Fade = saturate((ShadowParams2.y - Distance) / max(ShadowParams2.z, 0.001));
    return lerp(1.0, Visibility, Fade);
}

struct PSOutput {
    float4 Color    : SV_Target0;
    // Screen-space UV displacement since last frame, for TAA's resolve pass
    // to reproject history by (see TAA.hpp/TAAResolve.hlsl) - NDC delta
    // halved (NDC spans [-1,1], UV spans [0,1]) with Y flipped (NDC Y is up,
    // UV/texel Y is down).
    float2 Velocity : SV_Target1;
};

PSOutput PSMain(PSInput In) {
    // Every texture sample MULTIPLIES its matching constant factor (never
    // replaces it) - see MaterialBindings.hlsli. A material with no map
    // assigned is bound to a white (or flat-normal) placeholder by
    // MeshRenderer, so this is branch-free either way.
    const float3 Albedo   = AlbedoAndMetallic.xyz * AlbedoMap.Sample(AlbedoSampler, In.UV).rgb;
    const float Metallic  = AlbedoAndMetallic.w * MetallicMap.Sample(MetallicSampler, In.UV).r;
    const float Roughness = max(RoughnessAOAndPad.x * RoughnessMap.Sample(RoughnessSampler, In.UV).r, 0.045);
    const float AO        = RoughnessAOAndPad.y * AmbientOcclusionMap.Sample(AmbientOcclusionSampler, In.UV).r;
    const float3 Emissive = EmissiveAndPad.xyz * EmissiveMap.Sample(EmissiveSampler, In.UV).rgb;

    const float3 TangentSpaceNormal = NormalMap.Sample(NormalSampler, In.UV).xyz * 2.0 - 1.0;
    const float3 N = ApplyNormalMap(TangentSpaceNormal, In.WorldNormal, In.WorldTangent);

    const float3 V = normalize(CameraPositionAndPad.xyz - In.WorldPosition);
    const float3 L = normalize(-LightDirectionAndPad.xyz);

    // Dielectrics start near 0.04 reflectance at normal incidence; metals
    // tint their reflectance by their own albedo instead.
    const float3 F0 = lerp(float3(0.04, 0.04, 0.04), Albedo, Metallic);

    // Shadowing uses the interpolated geometric normal, not the normal-mapped
    // one: acne is a property of the surface's actual slope.
    const float3 GeometricNormal = normalize(In.WorldNormal);
    const float Shadow = ShadowVisibility(In.WorldPosition, GeometricNormal, dot(GeometricNormal, L), In.Position.xy);
    const float3 SunRadiance = LightColorAndIntensity.xyz * LightColorAndIntensity.w * Shadow;
    float3 DirectLight = EvaluateDirectLighting(N, V, L, Albedo, Metallic, Roughness, F0, SunRadiance);

    // Every point/spot light in this pixel's own screen tile (see
    // LightCulling.hlsli) - Forward+: culled by a compute pass earlier this
    // frame (Code/Shaders/LightCulling.hlsl) instead of looping every scene
    // light at every pixel. No shadows from these; only the one directional
    // light casts them.
    const uint2 TileCoord     = uint2(In.Position.xy) / (uint) TileGridAndSize.z;
    const uint TileIndex      = TileCoord.y * (uint) TileGridAndSize.x + TileCoord.x;
    const uint TileLightCount = min(TileLightGrid.Load(TileIndex * 4), XEN_MAX_LIGHTS_PER_TILE);

    for (uint i = 0; i < TileLightCount; ++i) {
        const uint LightIndex = LightIndexList.Load((TileIndex * XEN_MAX_LIGHTS_PER_TILE + i) * 4);
        const Light Lt = Lights[LightIndex];

        const float3 ToLight = Lt.PositionAndRange.xyz - In.WorldPosition;
        const float Distance = length(ToLight);
        const float3 Li      = ToLight / max(Distance, 0.0001);

        float Attenuation = DistanceAttenuation(Distance, Lt.PositionAndRange.w);
        if (Lt.DirectionAndType.w == XEN_LIGHT_TYPE_SPOT) {
            Attenuation *= SpotAttenuation(Li, Lt.DirectionAndType.xyz, Lt.ConeAnglesAndPad.x, Lt.ConeAnglesAndPad.y);
        }
        if (Attenuation <= 0.0) continue;

        const float3 PunctualRadiance = Lt.ColorAndIntensity.xyz * Lt.ColorAndIntensity.w * Attenuation;
        DirectLight += EvaluateDirectLighting(N, V, Li, Albedo, Metallic, Roughness, F0, PunctualRadiance);
    }

    // Image-based lighting, split-sum style: the environment maps supply the
    // incoming light, BrdfLUT supplies how much of it this material reflects
    // at this view angle and roughness (F0 * scale + bias).
    //
    // EnvironmentMap is the GGX-prefiltered specular cube (EnvironmentBaker),
    // one roughness level per mip (MipForRoughness inverts the baker's
    // RoughnessForMip), so roughness picks a mip: blurry reflections cost one
    // sample. IrradianceMap is the cosine-convolved diffuse lighting, already
    // integrated over the hemisphere. Both are sampled by direction, and
    // SampleLevel picks the mip explicitly. A scene with no environment binds
    // a tiny placeholder sky cube for both (one mip, so the LOD clamps to 0).
    const float NdotV = max(dot(N, V), 0.0);
    const float3 FIndirect = FresnelSchlickRoughness(NdotV, F0, Roughness);
    const float3 KDiffuseIndirect = (1.0 - FIndirect) * (1.0 - Metallic);

    // A cheap, deliberately non-physical "contact darkening": this light's
    // shadow ray only ever tells us about direct light, not the sky's, so
    // strictly this shouldn't touch ambient at all - but a single low-res
    // dynamic shadow map is already a crude stand-in for real occlusion/GI,
    // and without this a strong HDRI's ambient alone can keep a fully
    // shadowed diffuse surface nearly as bright as a lit one (see
    // DirectionalLightComponent::ShadowAmbientDarkening). Never applied to
    // specular below - a reflective surface legitimately keeps showing the
    // environment regardless of whether the sun itself is occluded here.
    const float AmbientShadow = lerp(1.0 - ShadowParams2.w, 1.0, Shadow);

    // Screen-space ambient occlusion (see SSAO.hpp): unlike AmbientShadow
    // above, this models real geometric visibility of the environment, not
    // one light's own shadow ray, so it legitimately darkens both diffuse
    // and specular ambient - a mirror in a corner still shows less of the
    // sky than one out in the open. Sampled by screen position, not this
    // surface's UV; white (1.0, no occlusion) wherever SSAO is off or
    // unavailable (see MeshRenderer.cpp).
    const float2 ScreenUV = In.Position.xy * InvScreenSizeAndPad.xy;
    const float SSAOTerm  = SSAOMap.SampleLevel(SSAOSampler, ScreenUV, 0);

    const float3 Irradiance = IrradianceMap.SampleLevel(IrradianceSampler, N, 0).rgb;
    const float3 DiffuseIBL = Irradiance * Albedo * KDiffuseIndirect * AO * AmbientShadow * SSAOTerm;

    uint EnvWidth, EnvHeight, EnvLevels;
    EnvironmentMap.GetDimensions(0, EnvWidth, EnvHeight, EnvLevels);
    const float EnvLod = MipForRoughness(Roughness, EnvLevels);

    const float3 R = reflect(-V, N);
    const float3 PrefilteredColor = EnvironmentMap.SampleLevel(EnvironmentSampler, R, EnvLod).rgb;
    const float2 EnvBRDF = BrdfLUT.SampleLevel(BrdfLUTSampler, float2(NdotV, Roughness), 0).rg;
    const float3 SpecularIBL = PrefilteredColor * (F0 * EnvBRDF.x + EnvBRDF.y) * AO * SSAOTerm;

    const float3 Ambient = DiffuseIBL + SpecularIBL;

    float3 Color = Ambient + DirectLight + Emissive;

    PSOutput Out;

    // Linear HDR, untonemapped: MeshRenderer renders into an offscreen
    // RGBA16F target, and the post-process composite pass (exposure, bloom,
    // ACES + gamma - see Tonemap.hlsli) is what maps it down to the swap
    // chain's LDR format. Alpha marks "a pixel this pass actually wrote" -
    // the composite blends over whatever was in the target before by it, so
    // it must be 1 here even though nothing downstream reads Color's alpha.
    Out.Color = float4(Color, 1.0);

    const float2 CurrNdc = In.CurrClip.xy / In.CurrClip.w;
    const float2 PrevNdc = In.PrevClip.xy / In.PrevClip.w;
    Out.Velocity = (CurrNdc - PrevNdc) * float2(0.5, -0.5);

    return Out;
}
