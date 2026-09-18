# Changelog

## 2026-09-18

### Added

- Mesh loading via glTF/GLB (using the vendored `cgltf` single-header parser), replacing the old custom `.xmesh` binary format.
- `plane`, `sphere`, and `cylinder` primitive generators in `generate_primitive_meshes.py` (previously cube-only), each emitting a self-contained `.gltf` with an embedded base64 buffer.
- Working `.pakignore` filtering in PAKTool's `pack` command: glob patterns (`*`, `?`), `#` comments, and no-`/` patterns matching by filename at any depth. The patterns were previously parsed and printed but never actually applied to the scanned file list.

### Changed

- The engine now adopts glTF's right-handed, +Y-up, -Z-forward coordinate convention directly (`CameraComponent`, `DirectionalLightComponent`) instead of converting on import.
- D3D12 rasterizer front-face winding mapping updated to match the new right-handed camera pipeline (a mesh's screen-space winding depends on the view/projection pipeline's handedness, not just the NDC-to-viewport Y flip).

### Removed

- Custom `.xmesh` binary mesh format, its header (`MeshAsset.hpp`), and the old sample `cube.xmesh` asset.

### Fixed

- `compile_engine_shaders.py` crashing under MSBuild's captured console codepage due to a non-ASCII character in a print statement.
- PAKTool's `pack` command failing when `.pakignore` patterns filter out every file in a content directory (e.g. a placeholder-only `Engine/Environment`, whose only files are `.keep` markers now excluded by the repo's own `.pakignore`) - that's the correct, expected outcome and now produces a valid 0-asset pak instead of a build-breaking error.
