# Changelog

## 2026-09-19

### Added

- `RHI::FrameStats::TriangleCount`, tallied per Draw/DrawIndexed call for TriangleList/TriangleStrip topologies - shown in Demo.PBR's Frame Stats window.
- Resident-byte tracking on `TextureCache`/`MeshCache` (`GetResidentBytes()`) and a new `IRenderDevice::GetMemoryStats()` (GPU allocated/reserved/usage/budget via D3D12MA's budget query, plus process RAM) - both exposed to any demo's debug UI, not folded into per-frame `FrameStats`. Demo.PBR's Frame Stats window shows both.
- Real image-based lighting in `PBR.hlsl`, replacing the old flat `0.03 * Albedo` ambient constant (which gave a `Metallic = 1` surface nothing to show at all - a full metal has zero diffuse response by definition): a Radiance `.hdr` equirectangular environment map supplies the incoming light (sampled at `N` for diffuse, at the reflection vector for specular), and a baked split-sum BRDF LUT supplies how much of it the material reflects (`F0 * scale + bias`).
  - `EnvironmentComponent` (scene-wide, first one found wins - same rule as the directional light) references the `.hdr` asset; a scene with none binds a dim two-tone placeholder sky, so the shader never branches on "is there an environment".
  - `TextureCache` decodes `.hdr` (detected from content, `stbi_loadf`) to packed RGBA16F, clamped to half-float range so a bright sun disc can't become infinity.
  - The BRDF LUT is a 256x256 RG16F texture rendered once at `MeshRenderer::Initialize` by `Code/Shaders/BRDFIntegrate.hlsl` (GGX importance-sampled integration, fullscreen triangle from `SV_VertexID`, no bindings).
  - Two new scene-level binding slots, `Environment` (t5/s5) and `BrdfLut` (t6/s6), bound once per frame rather than per draw - see `MaterialBindings.hlsli`/`.hpp`.
  - `IRenderDevice::SubmitAndWait()`: execute a `CommandBuffer` immediately and block until the GPU finishes, outside the `BeginFrame`/`EndFrame` cycle and with no present - what a one-shot bake pass needs, the same way `UploadTexture` is already synchronous.
  - `Scripts/generate_test_hdri.py` writes a synthetic sky+sun test map (not shipped content; Demo.PBR points at it).
  - Prefiltered, not raw: `EnvironmentBaker` (`EnvironmentBaker.hpp/.cpp`) bakes the environment on the GPU, the first frame a scene has one, into (a) a GGX-prefiltered specular mip chain - mip `m` is roughness `m / (mips - 1)`, so `PBR.hlsl` reads a blurry reflection with one sample at `LOD = Roughness * (levels - 1)` - and (b) a small 64x32 cosine-convolved irradiance map for diffuse. `PrefilterEnvironment.hlsl`/`IrradianceConvolve.hlsl` importance-sample the source at a mip chosen from each sample's pdf (filtered importance sampling), so a bright sun disc spreads smoothly instead of speckling; `TextureCache` gives an HDR image a CPU box-filtered mip pyramid for exactly that. The baked pair is rebaked if the scene's environment changes; a scene with none binds the placeholder sky for both maps (`Environment` t5, `Irradiance` t6, `BrdfLut` t7).
  - D3D12 render targets can now target a single mip (`ColorAttachment::MipLevel`, previously ignored): a color-target texture gets one RTV per mip, and the pass viewport is that mip's own extent. The offscreen RTV heap grew from 32 to 128 slots to fit mip chains.
  - Shared shader code moved into includes, per the standardized-shader-input direction: `Include/Common.hlsli` (`PI`, the equirect direction<->UV mapping, Hammersley/GGX sampling) and `Include/Fullscreen.hlsli` (the `SV_VertexID` fullscreen triangle); `BRDFIntegrate.hlsl` and `PBR.hlsl` now use them.

### Fixed

- `TextureCache` always created textures as `RGBA8_UNORM`, with a cache-*wide* `SrgbTextures` config flag nothing ever set - so a color texture like an albedo/emissive map (authored in sRGB) was sampled as raw, un-linearized bytes and then gamma-encoded a second time by `PBR.hlsl`'s own tonemap pass, desaturating and darkening it. Since one `TextureCache` is shared between sprites (which want raw passthrough - displayed as-authored, no lighting) and PBR material channels (where albedo/emissive need sRGB decode but normal/metallic-roughness/occlusion must NOT be decoded, per glTF), a cache-wide flag couldn't express this correctly. Replaced with a per-`Acquire`/`Preload` `Srgb` parameter; `PBRMaterialComponent` now requests it only for its Albedo/Emissive channels.
- `D3D12RenderDevice::UploadTexture` hardcoded a 4-bytes-per-pixel source row pitch ("RGBA8 - the only format the texture cache uploads today"), which would have silently corrupted any wider-format upload (an HDR texture reads the wrong byte range on every row). The pitch now comes from the resource's actual format.
- `D3D12RenderDevice::UploadTexture` transitioned only the uploaded subresource from a hardcoded `COMMON`, then recorded one state for the whole resource - wrong for a texture uploaded one mip at a time (the first-uploaded mip ended up in a different state from the rest). It now transitions the whole resource from its tracked state.
- `PipelineLayoutDesc::MAX_BINDINGS` was 16 with an unchecked `Binding()` - MeshRenderer's layout is now 17 (3 cbuffers + 7 texture/sampler pairs), so the 17th binding would have written out of bounds. Raised to 32.

## 2026-09-18

### Added

- Mesh loading via glTF/GLB (using the vendored `cgltf` single-header parser), replacing the old custom `.xmesh` binary format.
- `plane`, `sphere`, and `cylinder` primitive generators in `generate_primitive_meshes.py` (previously cube-only), each emitting a self-contained `.gltf` with an embedded base64 buffer.
- Working `.pakignore` filtering in PAKTool's `pack` command: glob patterns (`*`, `?`), `#` comments, and no-`/` patterns matching by filename at any depth. The patterns were previously parsed and printed but never actually applied to the scanned file list.
- A `DebugUI` layer (Dear ImGui, docking branch) that any `Game` subclass can draw into from `OnRender` with ordinary `ImGui::` calls - `Game::BeginFrame`/`EndFrame` already bracket a frame, and Win32 input is forwarded/withheld correctly (dragging a debug window no longer also moves the game camera or spins the mouse-look). Compiled out entirely in release builds (`DebugUI.hpp`'s `XEN_WITH_DEBUG_UI`, on by default whenever `NDEBUG` isn't defined). `Demo.PBR` now shows a small "Frame Stats" window as a working example.
- Texture support for `PBRMaterialComponent`: albedo, normal, metallic-roughness (glTF's own G=roughness/B=metallic packing), ambient-occlusion, and emissive maps, each optional - an unassigned channel falls back to a white (or flat-normal) placeholder that multiplies through as the identity, so the existing constant-factor-only workflow keeps working unchanged.
- A standardized binding-slot convention for mesh-rendering shaders (`Code/Shaders/Include/MaterialBindings.hlsli`, mirrored in `Code/Modules/Xen/MaterialBindings.hpp`): `b0`/`b1`/`b2` for frame/object/material data and `t0-t4`/`s0-s4` for the five material texture channels, fixed for every pipeline built this way - a normal map is always `t1`, regardless of which shader you're looking at.

### Changed

- The engine now adopts glTF's right-handed, +Y-up, -Z-forward coordinate convention directly (`CameraComponent`, `DirectionalLightComponent`) instead of converting on import.
- D3D12 rasterizer front-face winding mapping updated to match the new right-handed camera pipeline (a mesh's screen-space winding depends on the view/projection pipeline's handedness, not just the NDC-to-viewport Y flip).

### Removed

- Custom `.xmesh` binary mesh format, its header (`MeshAsset.hpp`), and the old sample `cube.xmesh` asset.

### Fixed

- `compile_engine_shaders.py` crashing under MSBuild's captured console codepage due to a non-ASCII character in a print statement.
- PAKTool's `pack` command failing when `.pakignore` patterns filter out every file in a content directory (e.g. a placeholder-only `Engine/Environment`, whose only files are `.keep` markers now excluded by the repo's own `.pakignore`) - that's the correct, expected outcome and now produces a valid 0-asset pak instead of a build-breaking error.
- `D3D12RenderDevice::CreateSampler`'s point/linear filter encoding: it packed `(min, mag, mip)` into a plain 3-bit `0-7` value, but `D3D12_FILTER`'s real point/linear values aren't contiguous (min is bit 4, mag bit 2, mip bit 0, e.g. `D3D12_FILTER_MIN_MAG_MIP_LINEAR` is `0x15`, not `0x7`). Every sampler in the engine had used `MipMode::None` until now, which happened to alias onto a value the driver silently tolerated; the first `MipMode::Linear` (trilinear) sampler - `MeshRenderer`'s new material sampler - produced a genuinely invalid filter that removed the D3D12 device outright (confirmed via the debug layer: "CreateSampler2: Filter unrecognized").
- `PBR.hlsl`'s new normal mapping: a mesh with no authored `TANGENT` attribute (glTF's own optional field - most DCC exporters, including Blender's default glTF export, omit it) arrived with an all-zero tangent, and `normalize()`-ing that is NaN, poisoning every lighting term for the whole mesh. Falls back to an arbitrary-but-valid tangent basis instead, which is exactly correct for the common case (no normal map assigned) and merely arbitrary (not NaN) if one ever is assigned to a tangent-less mesh.
