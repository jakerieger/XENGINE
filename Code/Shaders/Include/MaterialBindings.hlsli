// Standardized root-signature binding slots for XEN's forward-rendered mesh
// shaders (MeshRenderer's pipelines - PBR.hlsl today, any future material
// shader authored the same way). Fixing these once means a normal map is
// always t1/s1, a light is always read from b0, etc. regardless of which
// shader you're looking at - no per-shader lookup required, and the C++ side
// (MeshRenderer's PipelineLayoutDesc, Code/Modules/Xen/MaterialBindings.hpp)
// binds to these exact same numbers.
//
// Keep this file and MaterialBindings.hpp in sync by hand - there is no
// shared code generation between HLSL and C++ here, the same tradeoff
// MeshAsset.hpp/generate_primitive_meshes.py already make.
//
// New material shaders should reuse this scheme rather than inventing their
// own: b0/b1 are meant to be shared across every mesh-rendering pipeline
// (MeshRenderer only rebinds them once per frame/object, not per-pipeline),
// and b2 plus the six texture slots are the material's own - a shader that
// needs fewer than six textures just leaves the unused ones bound to the
// engine's default placeholder (see MeshRenderer's _WhiteTexture/
// _FlatNormalTexture) rather than skipping the binding, so every material
// pipeline built this way can share one binding shape.

#define XEN_FRAME_REGISTER    b0  // FrameData: per-frame (view-projection, camera, light) - one bind per frame
#define XEN_OBJECT_REGISTER   b1  // ObjectData: per-object (model matrix) - one bind per draw
#define XEN_MATERIAL_REGISTER b2  // MaterialData: per-material scalar factors - one bind per draw

// Texture/sampler slots share their number across register spaces (t#/s#),
// matching Code/Modules/Xen/CommandBuffer.hpp's BindTexture(Slot, Tex,
// Sampler), which resolves both through one Slot value.
//
// A texture's sampled value always MULTIPLIES the matching MaterialData
// scalar factor (never replaces it), so "no texture assigned" and "texture
// bound but at 1.0/white" are the same thing. That's also why every
// material always binds all six: multiplying by a white (or flat-normal)
// placeholder is a cheap, branch-free identity operation, so there's no
// per-material shader variant to compile or select.
#define XEN_ALBEDO_TEX_REGISTER      t0
#define XEN_ALBEDO_SAMPLER_REGISTER  s0

#define XEN_NORMAL_TEX_REGISTER      t1  // Tangent-space, sampled.xyz * 2 - 1
#define XEN_NORMAL_SAMPLER_REGISTER  s1

// Roughness and metallic are separate single-channel maps (read from .r),
// not glTF's packed G/B metallic-roughness texture - a material can supply
// either, both, or neither independently, and there's no need to author (or
// re-pack) a combined texture just to test one.
#define XEN_ROUGHNESS_TEX_REGISTER      t2
#define XEN_ROUGHNESS_SAMPLER_REGISTER  s2

#define XEN_METALLIC_TEX_REGISTER      t3
#define XEN_METALLIC_SAMPLER_REGISTER  s3

#define XEN_AO_TEX_REGISTER      t4
#define XEN_AO_SAMPLER_REGISTER  s4

#define XEN_EMISSIVE_TEX_REGISTER      t5
#define XEN_EMISSIVE_SAMPLER_REGISTER  s5

// Scene-level slots: not a material's own (t0-t5 above are), and they don't
// vary per draw - MeshRenderer binds them once per frame, before the per-actor
// loop. Every pipeline built on this convention declares them.
#define XEN_ENVIRONMENT_TEX_REGISTER      t6  // GGX-prefiltered environment CUBE (RGBA16F), mip = roughness (see Common.hlsli's RoughnessForMip), sampled by direction
#define XEN_ENVIRONMENT_SAMPLER_REGISTER  s6

#define XEN_IRRADIANCE_TEX_REGISTER      t7  // Cosine-convolved diffuse-lighting CUBE (RGBA16F), sampled by the surface normal
#define XEN_IRRADIANCE_SAMPLER_REGISTER  s7

#define XEN_BRDF_LUT_TEX_REGISTER      t8  // RG16F split-sum LUT, indexed by (NdotV, Roughness) -> (scale, bias)
#define XEN_BRDF_LUT_SAMPLER_REGISTER  s8

#define XEN_SHADOW_MAP_TEX_REGISTER      t9  // Directional-light shadow map (D32_FLOAT), sampled with a comparison sampler
#define XEN_SHADOW_MAP_SAMPLER_REGISTER  s9  // SamplerComparisonState (LessEqual, clamp)
