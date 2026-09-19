# Changelog

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
