//
// TextureCache.cpp
//

#include "TextureCache.hpp"
#include "Exception.hpp"

#include <PAK/AssetRegistry.hpp>

#include <stb_image.h>
#include <format>
#include <ranges>

namespace Xen {
    TextureCache::~TextureCache() {
        Clear();
    }

    TextureCache::Entry TextureCache::CreateEntry(const AssetID ID, const u32 InitialRefCount) {
        if (!_Assets->Contains(ID)) {
            _ThrowEngineException(EngineException, std::format("texture asset {} not found", ID.Value));
        }

        const PAK::AssetBuffer Encoded = _Assets->Load(ID);

        Entry E;
        std::vector<u8> Pixels = DecodeImage(Encoded.Data(), Encoded.Size(), E.Info);

        RHI::TextureDesc Desc;
        Desc.Type   = RHI::TextureType::Texture2D;
        Desc.Fmt    = _Config.SrgbTextures ? RHI::Format::RGBA8_SRGB : RHI::Format::RGBA8_UNORM;
        Desc.Width  = E.Info.Width;
        Desc.Height = E.Info.Height;
        // 0 means the full chain; 1 means no mips at all.
        Desc.MipLevels = _Config.GenerateMips ? 0 : 1;
        Desc.Usage     = RHI::TextureUsage::Sampled | RHI::TextureUsage::CopyDst;

        E.Handle = _Device->CreateTexture(Desc);
        if (!E.Handle.IsValid()) {
            _ThrowEngineException(EngineException, std::format("GPU texture creation failed for asset {}", ID.Value));
        }

        RHI::TextureUploadDesc Upload;
        Upload.Data     = Pixels.data();
        Upload.DataSize = Pixels.size();
        Upload.Width    = E.Info.Width;
        Upload.Height   = E.Info.Height;
        _Device->UploadTexture(E.Handle, Upload);

        E.RefCount = InitialRefCount;

        _InfoByHandle[E.Handle.ID] = E.Info;
        if (_Config.RetainPixels) _PixelsByHandle[E.Handle.ID] = std::move(Pixels);

        return E;
    }

    TextureHandle TextureCache::Acquire(const AssetID ID) {
        if (!ID.IsValid()) return {};

        if (const auto It = _Entries.find(ID.Value); It != _Entries.end()) {
            ++It->second.RefCount;
            return It->second.Handle;
        }

        const Entry E = CreateEntry(ID, 1);
        _Entries.emplace(ID.Value, E);
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

        _InfoByHandle.erase(HandleID);
        _PixelsByHandle.erase(HandleID);
        _Entries.erase(It);
    }

    void TextureCache::Preload(const AssetID ID) {
        if (!ID.IsValid() || _Entries.contains(ID.Value)) return;
        _Entries.emplace(ID.Value, CreateEntry(ID, 0));
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
    }

    const std::vector<u8>* TextureCache::GetPixels(const TextureHandle Handle) const {
        const auto It = _PixelsByHandle.find(Handle.ID);
        return It != _PixelsByHandle.end() ? &It->second : nullptr;
    }

    std::vector<u8> TextureCache::DecodeImage(const u8* Bytes, const size_t Size, TextureInfo& OutInfo) {
        int W = 0, H = 0, Channels = 0;

        stbi_uc* Pixels = stbi_load_from_memory(Bytes, CAST<int>(Size), &W, &H, &Channels, STBI_rgb_alpha);
        if (!Pixels) {
            _ThrowEngineException(EngineException, std::format("image decode failed: {}", stbi_failure_reason()));
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