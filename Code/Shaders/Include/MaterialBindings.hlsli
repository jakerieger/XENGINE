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
// and b2 plus the five texture slots are the material's own - a shader that
// needs fewer than five textures just leaves the unused ones bound to the
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
// Channel semantics match glTF's own PBR metallic-roughness model exactly,
// since the engine's mesh assets are glTF-native (see MeshCache.cpp) - a
// texture's sampled value always MULTIPLIES the matching MaterialData
// scalar factor (never replaces it), so "no texture assigned" and "texture
// bound but at 1.0/white" are the same thing. That's also why every
// material always binds all five: multiplying by a white (or flat-normal)
// placeholder is a cheap, branch-free identity operation, so there's no
// per-material shader variant to compile or select.
#define XEN_ALBEDO_TEX_REGISTER      t0
#define XEN_ALBEDO_SAMPLER_REGISTER  s0

#define XEN_NORMAL_TEX_REGISTER      t1  // Tangent-space, sampled.xyz * 2 - 1
#define XEN_NORMAL_SAMPLER_REGISTER  s1

#define XEN_METALROUGH_TEX_REGISTER      t2  // G = Roughness, B = Metallic (glTF's packing)
#define XEN_METALROUGH_SAMPLER_REGISTER  s2

#define XEN_AO_TEX_REGISTER      t3
#define XEN_AO_SAMPLER_REGISTER  s3

#define XEN_EMISSIVE_TEX_REGISTER      t4
#define XEN_EMISSIVE_SAMPLER_REGISTER  s4
