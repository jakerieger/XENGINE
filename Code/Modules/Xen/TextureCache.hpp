//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "RenderDevice.hpp"

#include <XenPAK/AssetID.hpp>

#include <unordered_map>

namespace Xen {
    namespace PAK {
        class AssetRegistry;
    }

    using TextureHandle = RHI::TextureHandle;

    struct TextureInfo {
        u32 Width {0};
        u32 Height {0};
        /// Resident VRAM bytes: RGBA8 content size, including the mip chain
        /// if TextureCache::Config::GenerateMips is on. This is the uploaded
        /// content size, not the backend's actual (padded/aligned) GPU
        /// allocation - close enough for a debug-UI stat, not exact accounting.
        u64 GpuBytes {0};
    };

    class TextureCache {
    public:
        struct Config {
            /// @brief Keep a CPU copy of every decoded image.
            ///
            /// Off by default. On, a 2048x2048 sheet costs 16 MB of RAM for
            /// its lifetime on top of the GPU copy. Turn it on only if
            /// something actually reads pixels back - per-pixel collision,
            /// say - and expect the memory.
            bool RetainPixels {false};

            /// @brief Generate a mip chain on upload.
            ///
            /// Off by default: sprites drawn near 1:1 gain nothing from mips,
            /// and mipping an atlas bleeds neighbouring tiles into the lower
            /// levels. Turn it on for heavily minified or zoomed-out content.
            bool GenerateMips {false};

            Config() {}
        };

        explicit TextureCache(PAK::AssetRegistry& Assets, RHI::IRenderDevice& Device, const Config& Cfg = {})
            : _Assets(&Assets), _Device(&Device), _Config(Cfg) {}
        ~TextureCache();

        TextureCache(const TextureCache&)            = delete;
        TextureCache& operator=(const TextureCache&) = delete;

        /// @brief Srgb picks the upload format: on, the texture is created
        /// RGBA8_SRGB and the GPU linearizes it on every sample (correct for
        /// a color map fed into lighting math - albedo/emissive); off (the
        /// default), it's RGBA8_UNORM and sampling returns the stored bytes
        /// unchanged (correct for a sprite, displayed as-authored with no
        /// lighting pass, or a data map - normal/metallic-roughness/
        /// occlusion - that was never sRGB-encoded to begin with).
        ///
        /// A Radiance .hdr image (an environment map) is detected from its
        /// content and always uploaded as RGBA16F, ignoring Srgb entirely -
        /// sRGB is an 8-bit gamma encoding, and a float texture is linear by
        /// definition.
        ///
        /// Only consulted the first time an AssetID becomes resident - an
        /// entry already cached (RefCount > 0) is reused as-is regardless of
        /// what Srgb is passed on a later Acquire. No current content reuses
        /// one image asset both ways, so this isn't handled specially.
        TextureHandle Acquire(AssetID ID, bool Srgb = false);
        void Release(AssetID ID);
        void Preload(AssetID ID, bool Srgb = false);

        NODISCARD bool IsResident(AssetID ID) const;
        NODISCARD TextureInfo GetInfo(TextureHandle Handle) const;
        NODISCARD size_t GetResidentCount() const { return _Entries.size(); }
        /// @brief Sum of every resident entry's TextureInfo::GpuBytes - O(1),
        /// maintained incrementally rather than summed on each call.
        NODISCARD u64 GetResidentBytes() const { return _ResidentBytes; }

        NODISCARD u32 GetRefCount(AssetID ID) const;
        void Clear();

    private:
        struct Entry {
            TextureHandle Handle {};
            TextureInfo Info {};
            u32 RefCount {0};
        };

    public:
        NODISCARD const std::vector<u8>* GetPixels(TextureHandle Handle) const;

    private:
        /// @brief Decodes an LDR image to RGBA8, or - if the bytes are a
        /// Radiance .hdr - to packed RGBA16F (OutIsHdr set), so the caller
        /// picks the matching GPU format and bytes-per-pixel. An HDR image
        /// also gets its full mip pyramid: the returned buffer is mip 0 and
        /// OutMipTail holds mips 1..N in order, each a box-filtered halving of
        /// the one before (empty for an LDR image).
        static std::vector<u8> DecodeImage(const u8* Bytes,
                                           size_t Size,
                                           TextureInfo& OutInfo,
                                           bool& OutIsHdr,
                                           std::vector<std::vector<u8>>& OutMipTail);
        Entry CreateEntry(AssetID ID, u32 InitialRefCount, bool Srgb);
        void FreeTexture(TextureHandle Handle) const;

        PAK::AssetRegistry* _Assets {nullptr};
        RHI::IRenderDevice* _Device {nullptr};
        Config _Config {};

        std::unordered_map<u32, std::vector<u8>> _PixelsByHandle;
        std::unordered_map<PAK::AssetIDValue, Entry> _Entries;
        std::unordered_map<u32, TextureInfo> _InfoByHandle;
        u32 _NextHandleID {1};  // 0 reserved for "invalid"
        u64 _ResidentBytes {0};
    };
}  // namespace Xen
