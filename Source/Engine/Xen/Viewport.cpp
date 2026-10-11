//
// Created by Jake Rieger on 9/17/2026.
//

#include "Viewport.hpp"

namespace Xen {
    Viewport::~Viewport() {
        Shutdown();
    }

    bool Viewport::Initialize(RHI::IRenderDevice& Device,
                              const u32 Width,
                              const u32 Height,
                              const RHI::Format ColorFormat,
                              const bool WithDepth,
                              const RHI::Format DepthFormat,
                              const f32 ClearR,
                              const f32 ClearG,
                              const f32 ClearB,
                              const f32 ClearA) {
        if (Width == 0 || Height == 0) return false;

        _Device      = &Device;
        _ColorFormat = ColorFormat;
        _DepthFormat = DepthFormat;
        _Width       = Width;
        _Height      = Height;
        _ClearColor[0] = ClearR;
        _ClearColor[1] = ClearG;
        _ClearColor[2] = ClearB;
        _ClearColor[3] = ClearA;

        RHI::TextureDesc ColorDesc;
        ColorDesc.Fmt       = ColorFormat;
        ColorDesc.Width     = Width;
        ColorDesc.Height    = Height;
        ColorDesc.MipLevels = 1;
        ColorDesc.Usage     = RHI::TextureUsage::ColorTarget | RHI::TextureUsage::Sampled;
        ColorDesc.OptimizedClear.Color[0] = ClearR;
        ColorDesc.OptimizedClear.Color[1] = ClearG;
        ColorDesc.OptimizedClear.Color[2] = ClearB;
        ColorDesc.OptimizedClear.Color[3] = ClearA;
        ColorDesc.DebugName = "Viewport Color";

        _ColorTarget = Device.CreateTexture(ColorDesc);
        if (!_ColorTarget.IsValid()) {
            _Device = nullptr;
            return false;
        }

        if (WithDepth) {
            RHI::TextureDesc DepthDesc;
            DepthDesc.Fmt       = DepthFormat;
            DepthDesc.Width     = Width;
            DepthDesc.Height    = Height;
            DepthDesc.MipLevels = 1;
            // Sampled, not just DepthTarget: SSAO reads this back as a
            // regular texture to reconstruct view-space position (see
            // MeshRenderer's depth prepass) - the same DepthTarget|Sampled
            // combination the shadow map already uses.
            DepthDesc.Usage     = RHI::TextureUsage::DepthTarget | RHI::TextureUsage::Sampled;
            DepthDesc.DebugName = "Viewport Depth";

            _DepthTarget = Device.CreateTexture(DepthDesc);
            if (!_DepthTarget.IsValid()) {
                Device.DestroyTexture(_ColorTarget);
                _ColorTarget = {};
                _Device      = nullptr;
                return false;
            }
        }

        return true;
    }

    void Viewport::Shutdown() {
        if (!_Device) return;

        if (_ColorTarget.IsValid()) _Device->DestroyTexture(_ColorTarget);
        if (_DepthTarget.IsValid()) _Device->DestroyTexture(_DepthTarget);

        _ColorTarget = {};
        _DepthTarget = {};
        _Device      = nullptr;
        _Width       = 0;
        _Height      = 0;
    }

    void Viewport::Resize(const u32 Width, const u32 Height) {
        if (!_Device) return;
        if (Width == 0 || Height == 0) return;
        if (Width == _Width && Height == _Height) return;

        RHI::IRenderDevice* Device = _Device;
        const RHI::Format ColorFmt = _ColorFormat;
        const RHI::Format DepthFmt = _DepthFormat;
        const bool WithDepth       = _DepthTarget.IsValid();

        if (_ColorTarget.IsValid()) Device->DestroyTexture(_ColorTarget);
        if (_DepthTarget.IsValid()) Device->DestroyTexture(_DepthTarget);

        RHI::TextureDesc ColorDesc;
        ColorDesc.Fmt       = ColorFmt;
        ColorDesc.Width     = Width;
        ColorDesc.Height    = Height;
        ColorDesc.MipLevels = 1;
        ColorDesc.Usage     = RHI::TextureUsage::ColorTarget | RHI::TextureUsage::Sampled;
        ColorDesc.OptimizedClear.Color[0] = _ClearColor[0];
        ColorDesc.OptimizedClear.Color[1] = _ClearColor[1];
        ColorDesc.OptimizedClear.Color[2] = _ClearColor[2];
        ColorDesc.OptimizedClear.Color[3] = _ClearColor[3];
        ColorDesc.DebugName = "Viewport Color";
        _ColorTarget        = Device->CreateTexture(ColorDesc);

        if (WithDepth) {
            RHI::TextureDesc DepthDesc;
            DepthDesc.Fmt       = DepthFmt;
            DepthDesc.Width     = Width;
            DepthDesc.Height    = Height;
            DepthDesc.MipLevels = 1;
            DepthDesc.Usage     = RHI::TextureUsage::DepthTarget | RHI::TextureUsage::Sampled;
            DepthDesc.DebugName = "Viewport Depth";
            _DepthTarget        = Device->CreateTexture(DepthDesc);
        }

        _Width  = Width;
        _Height = Height;
    }
}  // namespace Xen
