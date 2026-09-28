// Temporal anti-aliasing resolve: blends this frame's jittered color
// (SceneColor) against a reprojected running history, clamped to the
// current frame's own 3x3 neighborhood so a history sample that couldn't
// plausibly belong here (an occlusion/disocclusion, a fast-moving edge)
// gets pulled back rather than ghosting indefinitely. See TAA.hpp.

#include "Include/Fullscreen.hlsli"

cbuffer TAAParams : register(b0) {
    float4 Params;  // x = BlendFactor, y = Primed (0/1), zw unused
};

Texture2D<float4> SceneColor : register(t0);
SamplerState SceneColorSampler : register(s0);  // unused - read via Load, see TAA.hpp

Texture2D<float2> MotionVectors : register(t1);
SamplerState MotionVectorsSampler : register(s1);  // unused - read via Load

Texture2D<float4> History : register(t2);
SamplerState HistorySampler : register(s2);  // the only sampler this shader actually uses (bilinear reprojection)

VSOutput VSMain(uint VertexID : SV_VertexID) {
    return FullscreenVertex(VertexID, 0.0);
}

float4 PSMain(VSOutput In) : SV_Target {
    const int2 PixelCoord = int2(In.Position.xy);

    const float4 CurrentColor = SceneColor.Load(int3(PixelCoord, 0));
    const float2 Velocity     = MotionVectors.Load(int3(PixelCoord, 0));
    const float2 HistoryUV    = In.UV - Velocity;

    const bool Primed   = Params.y > 0.5;
    const bool InBounds = all(HistoryUV >= 0.0) && all(HistoryUV <= 1.0);
    if (!Primed || !InBounds) return CurrentColor;

    // AABB-clamp the reprojected history to this frame's own 3x3
    // neighborhood (read via Load - the exact, unfiltered texel grid - so
    // this doesn't itself smuggle in extra blurring): the standard, cheap
    // "neighborhood clamping" fix for ghosting - a history color far outside
    // what this frame's own local area looks like almost certainly belonged
    // to a surface that isn't here anymore.
    float3 Minimum = CurrentColor.rgb;
    float3 Maximum = CurrentColor.rgb;
    [unroll] for (int y = -1; y <= 1; ++y) {
        [unroll] for (int x = -1; x <= 1; ++x) {
            if (x == 0 && y == 0) continue;
            const float3 Tap = SceneColor.Load(int3(PixelCoord + int2(x, y), 0)).rgb;
            Minimum = min(Minimum, Tap);
            Maximum = max(Maximum, Tap);
        }
    }

    const float3 HistoryColor   = History.SampleLevel(HistorySampler, HistoryUV, 0).rgb;
    const float3 ClampedHistory = clamp(HistoryColor, Minimum, Maximum);

    // Alpha is coverage (see PostProcess.hpp/PBR.hlsl) - always this frame's
    // own, never blended: whether a pixel belongs to the 3D scene doesn't
    // need temporal smoothing, and blending it could leak a stale "covered"
    // flag onto a pixel the 3D pass no longer touches.
    return float4(lerp(ClampedHistory, CurrentColor.rgb, Params.x), CurrentColor.a);
}
