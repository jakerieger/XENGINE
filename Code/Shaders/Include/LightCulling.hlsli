// Forward+ tile-culling shared constants and buffer declarations - included
// by both Code/Shaders/LightCulling.hlsl (writes these) and PBR.hlsl (reads
// them), so the tile size, per-tile capacity, and u#-register numbers are
// written once. See Code/Modules/Xen/LightCulling.hpp for the full picture.

#ifndef XEN_LIGHTCULLING_HLSLI
#define XEN_LIGHTCULLING_HLSLI

// Kept in sync by hand with Code/Modules/Xen/LightCulling.hpp's
// TileSize/MaxLightsPerTile - same no-shared-codegen tradeoff as every other
// HLSL/C++ mirrored constant in this engine.
#define XEN_TILE_SIZE 16
#define XEN_MAX_LIGHTS_PER_TILE 64

#define XEN_LIGHT_INDEX_REGISTER u0
#define XEN_LIGHT_GRID_REGISTER  u1

// Raw (byte-address), not structured: a raw UAV needs no StructureByteStride
// bookkeeping on the C++ side (see D3D12RenderDevice::CreateBuffer) - every
// element is just a plain uint, addressed by byte offset (index * 4).
RWByteAddressBuffer LightIndexList : register(XEN_LIGHT_INDEX_REGISTER);  // TileCountX*TileCountY*XEN_MAX_LIGHTS_PER_TILE uints
RWByteAddressBuffer TileLightGrid  : register(XEN_LIGHT_GRID_REGISTER);  // TileCountX*TileCountY uints - light count per tile

#endif  // XEN_LIGHTCULLING_HLSLI
