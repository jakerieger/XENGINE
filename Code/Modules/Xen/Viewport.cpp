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
                              const RHI::Format ColorFormat) {
        if (Width == 0 || Height == 0) return false;

        _Device      = &Device;
        _ColorFormat = ColorFormat;
        _Width       = Width;
        _Height      = Height;

        RHI::TextureDesc Desc;
        Desc.Fmt        = ColorFormat;
        Desc.Width      = Width;
        Desc.Height     = Height;
        Desc.MipLevels  = 1;
        Desc.Usage      = RHI::TextureUsage::ColorTarget | RHI::TextureUsage::Sampled;
        Desc.DebugName  = "Viewport";

        _ColorTarget = Device.CreateTexture(Desc);
        if (!_ColorTarget.IsValid()) {
            _Device = nullptr;
            return false;
        }

        return true;
    }

    void Viewport::Shutdown() {
        if (!_Device) return;

        if (_ColorTarget.IsValid()) _Device->DestroyTexture(_ColorTarget);

        _ColorTarget = {};
        _Device      = nullptr;
        _Width       = 0;
        _Height      = 0;
    }

    void Viewport::Resize(const u32 Width, const u32 Height) {
        if (!_Device) return;
        if (Width == 0 || Height == 0) return;
        if (Width == _Width && Height == _Height) return;

        RHI::IRenderDevice* Device = _Device;
        const RHI::Format Format  = _ColorFormat;

        if (_ColorTarget.IsValid()) Device->DestroyTexture(_ColorTarget);

        RHI::TextureDesc Desc;
        Desc.Fmt       = Format;
        Desc.Width     = Width;
        Desc.Height    = Height;
        Desc.MipLevels = 1;
        Desc.Usage     = RHI::TextureUsage::ColorTarget | RHI::TextureUsage::Sampled;
        Desc.DebugName = "Viewport";

        _ColorTarget = Device->CreateTexture(Desc);
        _Width       = Width;
        _Height      = Height;
    }
}  // namespace Xen
