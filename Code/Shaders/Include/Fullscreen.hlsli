// A single triangle covering the whole viewport, generated from the vertex
// index alone (0,1,2 -> UVs (0,0),(2,0),(0,2)) - draw it with Draw(3) and no
// vertex or index buffer bound. UV (0,0) is the top-left texel, matching
// texture row 0. Shared by every full-screen bake/post pass (BRDFIntegrate,
// PrefilterEnvironment, IrradianceConvolve).

#ifndef XEN_FULLSCREEN_HLSLI
#define XEN_FULLSCREEN_HLSLI

struct VSOutput {
    float4 Position : SV_Position;
    float2 UV       : TEXCOORD0;
};

VSOutput VSMain(uint VertexID : SV_VertexID) {
    VSOutput Out;
    Out.UV       = float2((VertexID << 1) & 2, VertexID & 2);
    Out.Position = float4(Out.UV * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return Out;
}

#endif  // XEN_FULLSCREEN_HLSLI
