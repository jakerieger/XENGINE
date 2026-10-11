// A single triangle covering the whole viewport, generated from the vertex
// index alone (0,1,2 -> UVs (0,0),(2,0),(0,2)) - draw it with Draw(3) and no
// vertex or index buffer bound. UV (0,0) is the top-left texel, matching
// texture row 0. Shared by every full-screen bake/post pass (BRDFIntegrate,
// PrefilterEnvironment, IrradianceConvolve, Sky).
//
// Each shader declares its own VSMain (the compile script looks for it) and
// calls FullscreenVertex with the clip-space depth it wants: 0 for a plain
// bake pass, 1 (the far plane) for a sky drawn behind everything.

#ifndef XEN_FULLSCREEN_HLSLI
#define XEN_FULLSCREEN_HLSLI

struct VSOutput {
    float4 Position : SV_Position;
    float2 UV       : TEXCOORD0;
};

VSOutput FullscreenVertex(uint VertexID, float Depth) {
    VSOutput Out;
    Out.UV       = float2((VertexID << 1) & 2, VertexID & 2);
    Out.Position = float4(Out.UV * float2(2.0, -2.0) + float2(-1.0, 1.0), Depth, 1.0);
    return Out;
}

#endif  // XEN_FULLSCREEN_HLSLI
