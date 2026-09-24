// Forward+ tile light culling: one thread group per screen tile
// (XEN_TILE_SIZE x XEN_TILE_SIZE pixels, one thread per pixel). Reduces the
// tile's depth samples to a min/max NDC depth, unprojects the tile's 4
// screen corners at both depths into 8 world-space points, takes their AABB,
// then tests every scene light (LightData.hlsli's flat, capped array) as a
// world-space sphere against that box - survivors are compacted into
// LightIndexList/TileLightGrid (LightCulling.hlsli) for PBR.hlsl to read.
//
// World-space, not view-space frustum planes: reuses InvViewProjection the
// same way SSAO.hlsl's WorldPosFromDepth already does, so this needs no new
// matrices threaded through anywhere. See Code/Modules/Xen/LightCulling.hpp
// for the full picture (this is the engine's first real compute shader).
//
// Loaded as a precompiled DXIL asset out of a pak (see LightCulling.cpp).

#include "Include/LightData.hlsli"
#include "Include/LightCulling.hlsli"

cbuffer LightCullParams : register(b0) {
    row_major float4x4 InvViewProjection;
    float4 ScreenAndTileDim;  // x = screen width, y = screen height (pixels), z = tile count X, w = tile count Y
};

Texture2D<float> DepthTex : register(t0);
SamplerState PointSampler : register(s0);  // unused - depth is Load()'d, not sampled; declared to match the C++ bind

groupshared uint sMinDepthUint;
groupshared uint sMaxDepthUint;
groupshared float3 sBoxMin;
groupshared float3 sBoxMax;
groupshared uint sLightCount;
groupshared uint sLightIndices[XEN_MAX_LIGHTS_PER_TILE];

float3 UnprojectToWorld(float2 Ndc, float Depth) {
    const float4 World = mul(float4(Ndc, Depth, 1.0), InvViewProjection);
    return World.xyz / World.w;
}

[numthreads(XEN_TILE_SIZE, XEN_TILE_SIZE, 1)]
void CSMain(uint3 GroupID : SV_GroupID, uint3 GroupThreadID : SV_GroupThreadID, uint GroupIndex : SV_GroupIndex) {
    static const uint ThreadsPerGroup = XEN_TILE_SIZE * XEN_TILE_SIZE;

    if (GroupIndex == 0) {
        sMinDepthUint = asuint(1.0);
        sMaxDepthUint = 0;
        sLightCount   = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    const uint ScreenWidth  = (uint) ScreenAndTileDim.x;
    const uint ScreenHeight = (uint) ScreenAndTileDim.y;
    const uint TileCountX   = (uint) ScreenAndTileDim.z;

    const uint2 TileMinPixel = GroupID.xy * XEN_TILE_SIZE;
    const uint2 PixelCoord   = min(TileMinPixel + GroupThreadID.xy, uint2(ScreenWidth - 1, ScreenHeight - 1));

    // Depth is already in [0, 1] (D3D's NDC convention), so its bit pattern
    // via asuint() orders the same way the float does - no sign-flip trick
    // needed for InterlockedMin/Max the way a general float reduction would.
    const float Depth = DepthTex.Load(int3(PixelCoord, 0));
    if (Depth < 1.0) {  // skip the far plane / sky - see the header comment
        InterlockedMin(sMinDepthUint, asuint(Depth));
        InterlockedMax(sMaxDepthUint, asuint(Depth));
    }
    GroupMemoryBarrierWithGroupSync();

    const uint TileIndex = GroupID.y * TileCountX + GroupID.x;

    // Every sample in the tile was sky (sMinDepthUint never lowered from its
    // initial value): no geometry, so no light can be visible here - every
    // thread takes this branch identically (same shared values), so an
    // early return here is safe, nothing after this point needs the whole
    // group.
    if (sMinDepthUint > sMaxDepthUint) {
        if (GroupIndex == 0) TileLightGrid.Store(TileIndex * 4, 0);
        return;
    }

    if (GroupIndex == 0) {
        const float MinDepth = asfloat(sMinDepthUint);
        const float MaxDepth = asfloat(sMaxDepthUint);

        const float2 TileMinUv = float2(TileMinPixel) / float2(ScreenWidth, ScreenHeight);
        const float2 TileMaxUv = float2(min(TileMinPixel + XEN_TILE_SIZE, uint2(ScreenWidth, ScreenHeight))) /
                                 float2(ScreenWidth, ScreenHeight);
        // UV -> NDC: x is [0,1]->[-1,1] directly, y is flipped (UV grows
        // downward, NDC grows upward) - same convention SSAO.hlsl's
        // WorldPosFromDepth uses.
        const float2 NdcMin = float2(TileMinUv.x * 2.0 - 1.0, 1.0 - TileMaxUv.y * 2.0);
        const float2 NdcMax = float2(TileMaxUv.x * 2.0 - 1.0, 1.0 - TileMinUv.y * 2.0);

        // 8 world-space corners - the tile's 4 screen corners, unprojected
        // at both the nearest and farthest depth actually present in the
        // tile - their AABB is the culling volume every light below is
        // tested against.
        float3 BoxMin = float3(1e10, 1e10, 1e10);
        float3 BoxMax = float3(-1e10, -1e10, -1e10);
        [unroll]
        for (uint i = 0; i < 4; ++i) {
            const float2 Corner = float2((i & 1) ? NdcMax.x : NdcMin.x, (i & 2) ? NdcMax.y : NdcMin.y);
            const float3 Near = UnprojectToWorld(Corner, MinDepth);
            const float3 Far  = UnprojectToWorld(Corner, MaxDepth);
            BoxMin = min(BoxMin, min(Near, Far));
            BoxMax = max(BoxMax, max(Near, Far));
        }

        sBoxMin = BoxMin;
        sBoxMax = BoxMax;
    }
    GroupMemoryBarrierWithGroupSync();

    // Strided across the whole group rather than one thread looping over
    // every light: up to 128 lights, 256 threads, so most of the time every
    // light gets tested by its own thread in a single iteration.
    for (uint LightIndex = GroupIndex; LightIndex < LightCount; LightIndex += ThreadsPerGroup) {
        const Light Lt          = Lights[LightIndex];
        const float3 LightPos   = Lt.PositionAndRange.xyz;
        const float Radius      = Lt.PositionAndRange.w;
        const float3 ClosestPt  = clamp(LightPos, sBoxMin, sBoxMax);
        const float3 Delta      = ClosestPt - LightPos;

        // A spot light's cone is a subset of this sphere bound - the sphere
        // test may pass a spot tile that the exact cone wouldn't, which is
        // fine (PBR.hlsl's SpotAttenuation still rejects it there); it must
        // never reject a tile the cone actually reaches.
        if (dot(Delta, Delta) <= Radius * Radius) {
            uint Slot;
            InterlockedAdd(sLightCount, 1, Slot);
            if (Slot < XEN_MAX_LIGHTS_PER_TILE) sLightIndices[Slot] = LightIndex;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    const uint FinalCount = min(sLightCount, XEN_MAX_LIGHTS_PER_TILE);
    if (GroupIndex == 0) TileLightGrid.Store(TileIndex * 4, FinalCount);
    for (uint w = GroupIndex; w < FinalCount; w += ThreadsPerGroup) {
        LightIndexList.Store((TileIndex * XEN_MAX_LIGHTS_PER_TILE + w) * 4, sLightIndices[w]);
    }
}
