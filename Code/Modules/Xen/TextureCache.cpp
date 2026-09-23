//
// TextureCache.cpp
//

#include <Common/Log.hpp>
#include <Common/Exception.hpp>

#include "TextureCache.hpp"
#include "RadianceHdr.hpp"

#include <XenPAK/AssetRegistry.hpp>
#include <XenPAK/AssetBuffer.hpp>

#include <stb_image.h>
#include <algorithm>
#include <format>
#include <ranges>

namespace Xen {
    namespace {
        // Content-size mip chain sum, box-filter halving down to 1x1 - matches
        // how CreateTexture's MipLevels=0 ("full chain") is generated, close
        // enough for a debug-UI byte count (see TextureInfo::GpuBytes).
        u64 ComputeTextureBytes(const u32 Width, const u32 Height, const u64 BytesPerPixel, const bool WithMips) {
            if (!WithMips) return CAST<u64>(Width) * Height * BytesPerPixel;

            u64 Total = 0;
            u32 W = Width, H = Height;
            while (true) {
                Total += CAST<u64>(W) * H * BytesPerPixel;
                if (W == 1 && H == 1) break;
                W = std::max(1u, W / 2);
                H = std::max(1u, H / 2);
            }
            return Total;
        }

        // Matches D3D12RenderDevice::CreateTexture's own resolution of
        // TextureDesc::MipLevels == 0 ("the full chain") exactly - computed
        // here too (rather than relying on that sentinel) so TextureCache
        // knows up front how many levels MipGenerator needs to fill in.
        u32 ComputeMipLevels(const u32 Width, const u32 Height) {
            u32 Levels  = 1;
            u32 MaxSide = std::max(Width, Height);
            while (MaxSide > 1) {
                MaxSide >>= 1;
                ++Levels;
            }
            return Levels;
        }
    }  // namespace

    TextureCache::TextureCache(PAK::AssetRegistry& Assets, RHI::IRenderDevice& Device, const Config& Cfg)
        : _Assets(&Assets), _Device(&Device), _Config(Cfg) {
        if (_Config.GenerateMips) _MipGen.Initialize(Device, Assets);
    }

    TextureCache::~TextureCache() {
        Clear();
        _MipGen.Shutdown();
    }

    DecodedTexture TextureCache::DecodeAsset(const AssetID ID, const bool Srgb) const {
        if (!_Assets->Contains(ID)) {
            THROW_ENGINE_EXCEPTION(EngineException, std::format("texture asset {} not found", ID.Value));
        }

        const PAK::AssetBuffer Encoded = _Assets->Load(ID);

        DecodedTexture Out;
        Out.Srgb   = Srgb;
        Out.Pixels = DecodeImage(Encoded.Data(), Encoded.Size(), Out.Info, Out.IsHdr, Out.MipTail);

        // An HDR image is decoded to RGBA16F regardless of Srgb (sRGB is an
        // 8-bit gamma-curve encoding - a float texture is already linear by
        // definition, there's no sRGB flavor of it to pick). It always comes
        // with a full mip pyramid, built on the CPU in DecodeImage (see
        // MipTail): an environment map is the source EnvironmentBaker filters
        // by sample density, which needs its lower mips. GenerateMips (GPU-
        // side, see MipGenerator) is the separate path for LDR sprite/
        // material content - only meaningful if the generator actually
        // initialized, or UploadEntry will fall back to a single mip.
        const bool WithMips = Out.IsHdr || (_Config.GenerateMips && _MipGen.IsInitialized());
        Out.Info.GpuBytes   = ComputeTextureBytes(Out.Info.Width, Out.Info.Height, Out.IsHdr ? 8 : 4, WithMips);
        return Out;
    }

    TextureCache::Entry TextureCache::CreateEntry(const AssetID ID, const u32 InitialRefCount, const bool Srgb) {
        return UploadEntry(ID, DecodeAsset(ID, Srgb), InitialRefCount);
    }

    void TextureCache::AdoptPreloaded(const AssetID ID, DecodedTexture&& Decoded) {
        if (!ID.IsValid() || _Entries.contains(ID.Value)) return;

        const Entry E = UploadEntry(ID, std::move(Decoded), 0);
        _ResidentBytes += E.Info.GpuBytes;
        _Entries.emplace(ID.Value, E);
    }

    TextureCache::Entry
    TextureCache::UploadEntry(const AssetID ID, DecodedTexture&& Decoded, const u32 InitialRefCount) {
        Entry E;
        E.Info                       = Decoded.Info;
        const bool IsHdr             = Decoded.IsHdr;
        const bool Srgb              = Decoded.Srgb;
        std::vector<u8>& Pixels      = Decoded.Pixels;
        std::vector<std::vector<u8>>& MipTail = Decoded.MipTail;

        // GPU-generated mips (MipGenerator) need the generator actually
        // initialized - without it, a texture that asked for mips just gets
        // one, same as a game that left GenerateMips off entirely.
        const bool GenerateOnGpu = !IsHdr && _Config.GenerateMips && _MipGen.IsInitialized();
        const u32 Levels = IsHdr ? CAST<u32>(MipTail.size()) + 1
                            : (GenerateOnGpu ? ComputeMipLevels(E.Info.Width, E.Info.Height) : 1);

        RHI::TextureDesc Desc;
        Desc.Type      = RHI::TextureType::Texture2D;
        Desc.Fmt       = IsHdr ? RHI::Format::RGBA16_FLOAT : (Srgb ? RHI::Format::RGBA8_SRGB : RHI::Format::RGBA8_UNORM);
        Desc.Width     = E.Info.Width;
        Desc.Height    = E.Info.Height;
        Desc.MipLevels = Levels;
        Desc.Usage     = RHI::TextureUsage::Sampled | RHI::TextureUsage::CopyDst;
        // MipGenerator renders into mips 1.. as color targets - see
        // TextureCache.hpp's Config::GenerateMips.
        if (GenerateOnGpu) Desc.Usage = Desc.Usage | RHI::TextureUsage::ColorTarget;

        E.Handle = _Device->CreateTexture(Desc);
        if (!E.Handle.IsValid()) {
            THROW_ENGINE_EXCEPTION(EngineException, std::format("GPU texture creation failed for asset {}", ID.Value));
        }

        RHI::TextureUploadDesc Upload;
        Upload.Data     = Pixels.data();
        Upload.DataSize = Pixels.size();
        Upload.Width    = E.Info.Width;
        Upload.Height   = E.Info.Height;
        _Device->UploadTexture(E.Handle, Upload);

        for (size_t i = 0; i < MipTail.size(); ++i) {
            const u32 Level = CAST<u32>(i) + 1;

            RHI::TextureUploadDesc MipUpload;
            MipUpload.Data     = MipTail[i].data();
            MipUpload.DataSize = MipTail[i].size();
            MipUpload.MipLevel = Level;
            MipUpload.Width    = std::max(E.Info.Width >> Level, 1u);
            MipUpload.Height   = std::max(E.Info.Height >> Level, 1u);
            _Device->UploadTexture(E.Handle, MipUpload);
        }

        if (GenerateOnGpu) _MipGen.Generate(E.Handle, E.Info.Width, E.Info.Height, Levels, Srgb);

        E.RefCount = InitialRefCount;

        _InfoByHandle[E.Handle.ID] = E.Info;
        if (_Config.RetainPixels) _PixelsByHandle[E.Handle.ID] = std::move(Pixels);

        return E;
    }

    TextureHandle TextureCache::Acquire(const AssetID ID, const bool Srgb) {
        if (!ID.IsValid()) return {};

        if (const auto It = _Entries.find(ID.Value); It != _Entries.end()) {
            ++It->second.RefCount;
            return It->second.Handle;
        }

        const Entry E = CreateEntry(ID, 1, Srgb);
        _Entries.emplace(ID.Value, E);
        _ResidentBytes += E.Info.GpuBytes;
        return E.Handle;
    }

    void TextureCache::Release(const AssetID ID) {
        const auto It = _Entries.find(ID.Value);
        if (It == _Entries.end()) return;

        // Guard against an extra Release: underflowing an unsigned count would
        // wrap to ~4 billion and pin the texture forever.
        if (It->second.RefCount > 0) --It->second.RefCount;
        if (It->second.RefCount > 0) return;

        const u32 HandleID = It->second.Handle.ID;
        FreeTexture(It->second.Handle);

        _ResidentBytes -= It->second.Info.GpuBytes;
        _InfoByHandle.erase(HandleID);
        _PixelsByHandle.erase(HandleID);
        _Entries.erase(It);
    }

    void TextureCache::Preload(const AssetID ID, const bool Srgb) {
        if (!ID.IsValid() || _Entries.contains(ID.Value)) return;
        const Entry E = CreateEntry(ID, 0, Srgb);
        _ResidentBytes += E.Info.GpuBytes;
        _Entries.emplace(ID.Value, E);
    }

    bool TextureCache::IsResident(const AssetID ID) const {
        return _Entries.contains(ID.Value);
    }

    TextureInfo TextureCache::GetInfo(const TextureHandle Handle) const {
        const auto It = _InfoByHandle.find(Handle.ID);
        return It != _InfoByHandle.end() ? It->second : TextureInfo {};
    }

    u32 TextureCache::GetRefCount(const AssetID ID) const {
        const auto It = _Entries.find(ID.Value);
        return It != _Entries.end() ? It->second.RefCount : 0;
    }

    void TextureCache::Clear() {
        for (const Entry& E : _Entries | std::views::values) {
            FreeTexture(E.Handle);
        }

        _Entries.clear();
        _InfoByHandle.clear();
        _PixelsByHandle.clear();
        _ResidentBytes = 0;
    }

    const std::vector<u8>* TextureCache::GetPixels(const TextureHandle Handle) const {
        const auto It = _PixelsByHandle.find(Handle.ID);
        return It != _PixelsByHandle.end() ? &It->second : nullptr;
    }

    std::vector<u8> TextureCache::DecodeImage(const u8* Bytes,
                                              const size_t Size,
                                              TextureInfo& OutInfo,
                                              bool& OutIsHdr,
                                              std::vector<std::vector<u8>>& OutMipTail) const {
        OutIsHdr = IsRadianceHdr(Bytes, Size);
        if (OutIsHdr) {
            // Streamed and downsampled while decoding (see RadianceHdr.hpp),
            // so a huge capture never exists in memory at full float size.
            RadianceImage Image;
            std::string Error;
            if (!DecodeRadianceHdr(Bytes, Size, _Config.MaxHdrWidth, Image, Error)) {
                THROW_ENGINE_EXCEPTION(EngineException, std::format("HDR image decode failed: {}", Error));
            }

            OutInfo.Width  = Image.Width;
            OutInfo.Height = Image.Height;
            OutMipTail     = std::move(Image.MipTail);
            return std::move(Image.Mip0);
        }

        int W = 0, H = 0, Channels = 0;

        stbi_uc* Pixels = stbi_load_from_memory(Bytes, CAST<int>(Size), &W, &H, &Channels, STBI_rgb_alpha);
        if (!Pixels) {
            THROW_ENGINE_EXCEPTION(EngineException, std::format("image decode failed: {}", stbi_failure_reason()));
        }

        OutInfo.Width  = CAST<u32>(W);
        OutInfo.Height = CAST<u32>(H);

        const size_t ByteCount = CAST<size_t>(W) * CAST<size_t>(H) * 4;
        std::vector Rgba(Pixels, Pixels + ByteCount);
        stbi_image_free(Pixels);

        return Rgba;
    }

    void TextureCache::FreeTexture(const TextureHandle Handle) const {
        if (!Handle.IsValid() || !_Device) return;
        // Safe to call even if this texture was drawn with this frame: the
        // device defers the GPU delete past every in-flight frame.
        _Device->DestroyTexture(Handle);
    }
}  // namespace Xen