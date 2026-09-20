//
// Created by Jake Rieger on 9/20/2026.
//

#include "LoadingScreen.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Xen {
    namespace {
        // Embedded rather than loaded from a pak, for the same reason as
        // SpriteRenderer's: the loading screen has to work before any asset is
        // mounted. One quad (a triangle strip generated from SV_VertexID) per
        // shape, in pixels: a rounded rectangle (the bar's track and fill) or
        // a tapering arc (the spinner), antialiased with a one-pixel SDF edge.
        constexpr auto ShaderSource = R"(
cbuffer Params : register(b0) {
    float4 Screen;  // xy = target size in pixels
    float4 Rect;    // xy = center in pixels, zw = half size in pixels
    float4 Color;
    float4 Shape;   // x = kind (0 rounded rect, 1 arc), y = corner radius / ring thickness in px,
                    // z = arc start angle in radians, w = arc sweep as a fraction of a full turn
};

struct VSOutput {
    float4 Position : SV_Position;
    float2 Local : TEXCOORD0;  // pixels from the shape's center
};

static const float2 Corners[4] = {
    float2(-1.0, -1.0), float2(1.0, -1.0), float2(-1.0, 1.0), float2(1.0, 1.0)
};

VSOutput VSMain(uint VertexID : SV_VertexID) {
    // A pixel of margin on every side so the antialiased edge isn't clipped.
    const float2 Half  = Rect.zw + 1.0;
    const float2 Local = Corners[VertexID] * Half;
    const float2 Pixel = Rect.xy + Local;

    VSOutput Out;
    Out.Local    = Local;
    Out.Position = float4(Pixel.x / Screen.x * 2.0 - 1.0, 1.0 - Pixel.y / Screen.y * 2.0, 0.0, 1.0);
    return Out;
}

float4 PSMain(VSOutput In) : SV_Target {
    float Coverage;

    if (Shape.x < 0.5) {
        const float2 Q = abs(In.Local) - (Rect.zw - Shape.y);
        const float D  = length(max(Q, 0.0)) + min(max(Q.x, Q.y), 0.0) - Shape.y;
        Coverage       = saturate(0.5 - D);
    } else {
        const float Radius    = length(In.Local);
        const float Thickness = Shape.y;
        const float Ring      = saturate(0.5 - (abs(Radius - (Rect.z - Thickness * 0.5)) - Thickness * 0.5));

        const float TwoPi = 6.28318530718;
        const float Angle = fmod(atan2(In.Local.y, In.Local.x) - Shape.z + TwoPi * 2.0, TwoPi);
        const float Sweep = Shape.w * TwoPi;

        // Brightest at the arc's leading edge, fading to nothing at its tail -
        // reads as motion even in a single still frame.
        const float Along = saturate(1.0 - Angle / Sweep);
        Coverage          = Ring * (Angle < Sweep ? Along * Along : 0.0);
    }

    return float4(Color.rgb, Color.a * Coverage);
}
)";

        struct ShapeParams {
            f32 Screen[4];
            f32 Rect[4];
            f32 Color[4];
            f32 Shape[4];
        };
    }  // namespace

    LoadingScreen::~LoadingScreen() {
        Shutdown();
    }

    bool LoadingScreen::Initialize(RHI::IRenderDevice& Device) {
        RHI::ShaderDesc VertexDesc;
        VertexDesc.Stage      = RHI::ShaderStage::Vertex;
        VertexDesc.SourceType = RHI::ShaderSourceType::HLSL;
        VertexDesc.Code       = ShaderSource;
        VertexDesc.CodeSize   = std::strlen(ShaderSource);
        VertexDesc.EntryPoint = "VSMain";
        VertexDesc.DebugName  = "LoadingScreen.vs";

        RHI::ShaderDesc FragmentDesc = VertexDesc;
        FragmentDesc.Stage           = RHI::ShaderStage::Fragment;
        FragmentDesc.EntryPoint      = "PSMain";
        FragmentDesc.DebugName       = "LoadingScreen.ps";

        const RHI::ShaderHandle Vertex   = Device.CreateShader(VertexDesc);
        const RHI::ShaderHandle Fragment = Device.CreateShader(FragmentDesc);

        RHI::PipelineLayoutDesc LayoutDesc;
        LayoutDesc.Binding(0, RHI::BindingType::UniformBuffer, RHI::ShaderVisibility::All);
        LayoutDesc.DebugName = "LoadingScreen";
        _Layout              = Device.CreatePipelineLayout(LayoutDesc);

        if (Vertex.IsValid() && Fragment.IsValid() && _Layout.IsValid()) {
            RHI::GraphicsPipelineDesc PipelineDesc;
            PipelineDesc.VertexShader                 = Vertex;
            PipelineDesc.FragmentShader               = Fragment;
            PipelineDesc.PipelineLayout               = _Layout;
            PipelineDesc.Topology                     = RHI::PrimitiveTopology::TriangleStrip;
            PipelineDesc.Rasterizer.Cull              = RHI::CullMode::None;
            PipelineDesc.DepthStencil.DepthTestEnable = false;
            PipelineDesc.DepthStencil.DepthWriteEnable = false;
            PipelineDesc.Blend.Attachments[0]         = RHI::BlendAttachmentState::AlphaBlend();
            PipelineDesc.ColorAttachmentCount         = 1;
            // The swap chain itself is the target, not a Viewport.
            PipelineDesc.ColorFormats[0] = RHI::Format::BGRA8_UNORM;
            PipelineDesc.DebugName       = "LoadingScreen";
            _Pipeline                    = Device.CreateGraphicsPipeline(PipelineDesc);
        }

        if (Vertex.IsValid()) Device.DestroyShader(Vertex);
        if (Fragment.IsValid()) Device.DestroyShader(Fragment);

        _Device = &Device;
        if (!_Pipeline.IsValid()) {
            Shutdown();
            return false;
        }

        return true;
    }

    void LoadingScreen::Shutdown() {
        if (!_Device) return;

        if (_Pipeline.IsValid()) _Device->DestroyPipeline(_Pipeline);
        if (_Layout.IsValid()) _Device->DestroyPipelineLayout(_Layout);

        _Pipeline = {};
        _Layout   = {};
        _Device   = nullptr;
    }

    void LoadingScreen::Reset() {
        _DisplayedFraction = 0.0f;
        _SpinnerAngle      = 0.0f;
    }

    void LoadingScreen::DrawBackground() {
        DrawShapes({}, 0.0f, false);
    }

    void LoadingScreen::Draw(const LoadingProgress& Progress, const f32 DeltaTime) {
        DrawShapes(Progress, DeltaTime, true);
    }

    void LoadingScreen::DrawShapes(const LoadingProgress& Progress, const f32 DeltaTime, const bool WithShapes) {
        if (!_Device) return;

        const auto Width  = CAST<f32>(_Device->GetSwapChainWidth());
        const auto Height = CAST<f32>(_Device->GetSwapChainHeight());
        if (Width <= 0.0f || Height <= 0.0f) return;

        _Commands.Reset();
        _Commands.PushDebugGroup("Loading screen");

        RHI::RenderPassDesc Pass = RHI::RenderPassDesc::SwapChain(_Config.Background.R,
                                                                  _Config.Background.G,
                                                                  _Config.Background.B,
                                                                  _Config.Background.A);
        Pass.DebugName           = "Loading screen";
        _Commands.BeginRenderPass(Pass);

        if (WithShapes) {
            // Ease toward the target rather than jumping: assets finish in
            // lumps (one big one is most of the load), and a bar that snaps
            // looks broken where one that glides looks like it's working.
            const f32 Target = std::clamp(Progress.Fraction, 0.0f, 1.0f);
            _DisplayedFraction += (Target - _DisplayedFraction) * (1.0f - std::exp(-8.0f * DeltaTime));
            _DisplayedFraction = std::clamp(_DisplayedFraction, 0.0f, Target);

            constexpr f32 TwoPi = 6.28318530718f;
            _SpinnerAngle       = std::fmod(_SpinnerAngle + 4.5f * DeltaTime, TwoPi);

            _Commands.BindPipeline(_Pipeline);

            const auto DrawShape = [&](const f32 Cx,
                                       const f32 Cy,
                                       const f32 HalfW,
                                       const f32 HalfH,
                                       const Color& C,
                                       const f32 Kind,
                                       const f32 Radius,
                                       const f32 ArcStart,
                                       const f32 ArcSweep) {
                ShapeParams Params {};
                Params.Screen[0] = Width;
                Params.Screen[1] = Height;
                Params.Rect[0]   = Cx;
                Params.Rect[1]   = Cy;
                Params.Rect[2]   = HalfW;
                Params.Rect[3]   = HalfH;
                Params.Color[0]  = C.R;
                Params.Color[1]  = C.G;
                Params.Color[2]  = C.B;
                Params.Color[3]  = C.A;
                Params.Shape[0]  = Kind;
                Params.Shape[1]  = Radius;
                Params.Shape[2]  = ArcStart;
                Params.Shape[3]  = ArcSweep;

                _Commands.BindUniformBuffer(0, _Device->AllocateUniform(Params));
                _Commands.Draw(4);
            };

            const f32 CenterX = Width * 0.5f;

            // Spinner: a tapering three-quarter-turn arc.
            const f32 SpinnerRadius = std::clamp(Height * 0.05f, 20.0f, 56.0f);
            const f32 SpinnerY      = Height * 0.46f;
            DrawShape(CenterX,
                      SpinnerY,
                      SpinnerRadius,
                      SpinnerRadius,
                      _Config.Accent,
                      1.0f,
                      SpinnerRadius * 0.2f,
                      _SpinnerAngle,
                      0.75f);

            // Bar: a track, then the fill over it, clipped to the eased fraction.
            const f32 BarWidth  = std::clamp(Width * 0.34f, 220.0f, 640.0f);
            const f32 BarHeight = std::clamp(Height * 0.011f, 6.0f, 14.0f);
            const f32 BarY      = Height * 0.46f + SpinnerRadius + Height * 0.06f;
            const f32 Corner    = BarHeight * 0.5f;

            DrawShape(CenterX, BarY, BarWidth * 0.5f, BarHeight * 0.5f, _Config.Track, 0.0f, Corner, 0.0f, 0.0f);

            // Never narrower than the bar is tall: a rounded rect thinner than
            // its own corner radius collapses into a sliver.
            const f32 FillWidth = std::max(BarWidth * _DisplayedFraction, BarHeight);
            const f32 FillLeft  = CenterX - BarWidth * 0.5f;
            DrawShape(FillLeft + FillWidth * 0.5f,
                      BarY,
                      FillWidth * 0.5f,
                      BarHeight * 0.5f,
                      _Config.Accent,
                      0.0f,
                      Corner,
                      0.0f,
                      0.0f);
        }

        _Commands.EndRenderPass();
        _Commands.PopDebugGroup();
        _Device->Submit(_Commands);
    }
}  // namespace Xen
