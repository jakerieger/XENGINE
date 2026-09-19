//
// TextureCache.cpp
//

#include <Common/Log.hpp>
#include <Common/Exception.hpp>

#include "TextureCache.hpp"

#include <XenPAK/AssetRegistry.hpp>
#include <XenPAK/AssetBuffer.hpp>

#include <DirectXPackedVector.h>
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

        std::vector<u8> PackHalves(const float* Values, const size_t Count) {
            std::vector<u8> Halves(Count * sizeof(DirectX::PackedVector::HALF));
            DirectX::PackedVector::XMConvertFloatToHalfStream(RCAST<DirectX::PackedVector::HALF*>(Halves.data()),
                                                              sizeof(DirectX::PackedVector::HALF),
                                                              Values,
                                                              sizeof(float),
                                                              Count);
            return Halves;
        }
    }  // namespace

    TextureCache::~TextureCache() {
        Clear();
    }

    TextureCache::Entry TextureCache::CreateEntry(const AssetID ID, const u32 InitialRefCount, const bool Srgb) {
        if (!_Assets->Contains(ID)) {
            THROW_ENGINE_EXCEPTION(EngineException, std::format("texture asset {} not found", ID.Value));
        }

        const PAK::AssetBuffer Encoded = _Assets->Load(ID);

        Entry E;
        bool IsHdr = false;
        std::vector<std::vector<u8>> MipTail;
        std::vector<u8> Pixels = DecodeImage(Encoded.Data(), Encoded.Size(), E.Info, IsHdr, MipTail);

        // An HDR image is decoded to RGBA16F regardless of Srgb (sRGB is an
        // 8-bit gamma-curve encoding - a float texture is already linear by
        // definition, there's no sRGB flavor of it to pick). It always comes
        // with a full mip pyramid, built on the CPU in DecodeImage (see
        // MipTail): an environment map is the source EnvironmentBaker filters
        // by sample density, which needs its lower mips. GenerateMips is a
        // separate knob for LDR sprite/material content.
        const bool WithMips = IsHdr || _Config.GenerateMips;
        E.Info.GpuBytes     = ComputeTextureBytes(E.Info.Width, E.Info.Height, IsHdr ? 8 : 4, WithMips);

        RHI::TextureDesc Desc;
        Desc.Type   = RHI::TextureType::Texture2D;
        Desc.Fmt    = IsHdr ? RHI::Format::RGBA16_FLOAT : (Srgb ? RHI::Format::RGBA8_SRGB : RHI::Format::RGBA8_UNORM);
        Desc.Width  = E.Info.Width;
        Desc.Height = E.Info.Height;
        // 0 means the full chain; 1 means no mips at all - except an HDR
        // image, whose chain length is exactly what DecodeImage generated.
        Desc.MipLevels = IsHdr ? CAST<u32>(MipTail.size()) + 1 : (WithMips ? 0 : 1);
        Desc.Usage     = RHI::TextureUsage::Sampled | RHI::TextureUsage::CopyDst;

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
                                              std::vector<std::vector<u8>>& OutMipTail) {
        int W = 0, H = 0, Channels = 0;

        OutIsHdr = stbi_is_hdr_from_memory(Bytes, CAST<int>(Size)) != 0;
        if (OutIsHdr) {
            float* Floats = stbi_loadf_from_memory(Bytes, CAST<int>(Size), &W, &H, &Channels, STBI_rgb_alpha);
            if (!Floats) {
                THROW_ENGINE_EXCEPTION(EngineException,
                                       std::format("HDR image decode failed: {}", stbi_failure_reason()));
            }

            OutInfo.Width  = CAST<u32>(W);
            OutInfo.Height = CAST<u32>(H);

            // RGBA16F tops out at 65504 - a real capture's sun disc can be
            // brighter than that, and an out-of-range float converts to
            // infinity, which then poisons every lighting sum it touches.
            constexpr float HalfMax  = 65504.0f;
            const size_t ValueCount  = CAST<size_t>(W) * CAST<size_t>(H) * 4;
            for (size_t i = 0; i < ValueCount; ++i) Floats[i] = std::min(Floats[i], HalfMax);

            std::vector<u8> Halves = PackHalves(Floats, ValueCount);

            // Box-filter the pyramid in float (halving to 1x1, the same
            // 2x2-average-with-clamped-edges each level), packing each level
            // to half as it's produced so only two float levels are live at
            // once. Averaging radiance linearly is the right filter for HDR.
            std::vector<float> Previous(Floats, Floats + ValueCount);
            u32 PrevW = CAST<u32>(W), PrevH = CAST<u32>(H);
            stbi_image_free(Floats);

            while (PrevW > 1 || PrevH > 1) {
                const u32 NextW = std::max(PrevW / 2, 1u);
                const u32 NextH = std::max(PrevH / 2, 1u);
                std::vector<float> Next(CAST<size_t>(NextW) * NextH * 4);

                for (u32 y = 0; y < NextH; ++y) {
                    const u32 y0 = std::min(y * 2, PrevH - 1);
                    const u32 y1 = std::min(y * 2 + 1, PrevH - 1);
                    for (u32 x = 0; x < NextW; ++x) {
                        const u32 x0 = std::min(x * 2, PrevW - 1);
                        const u32 x1 = std::min(x * 2 + 1, PrevW - 1);
                        for (u32 c = 0; c < 4; ++c) {
                            const auto At = [&](const u32 px, const u32 py) {
                                return Previous[(CAST<size_t>(py) * PrevW + px) * 4 + c];
                            };
                            Next[(CAST<size_t>(y) * NextW + x) * 4 + c] =
                              0.25f * (At(x0, y0) + At(x1, y0) + At(x0, y1) + At(x1, y1));
                        }
                    }
                }

                OutMipTail.push_back(PackHalves(Next.data(), Next.size()));
                Previous = std::move(Next);
                PrevW    = NextW;
                PrevH    = NextH;
            }

            return Halves;
        }

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