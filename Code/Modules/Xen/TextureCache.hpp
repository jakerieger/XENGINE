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

            /// @brief Upload as sRGB so the GPU linearizes on sample.
            ///
            /// Only correct if the device was created with an sRGB swap chain
            /// format (DeviceDescriptor::EnableSrgbFramebuffer). Enabling one
            /// without the other gives washed-out or overly dark output, so
            /// both move together.
            bool SrgbTextures {false};

            Config() {}
        };

        explicit TextureCache(PAK::AssetRegistry& Assets, RHI::IRenderDevice& Device, const Config& Cfg = {})
            : _Assets(&Assets), _Device(&Device), _Config(Cfg) {}
        ~TextureCache();

        TextureCache(const TextureCache&)            = delete;
        TextureCache& operator=(const TextureCache&) = delete;

        TextureHandle Acquire(AssetID ID);
        void Release(AssetID ID);
        void Preload(AssetID ID);

        NODISCARD bool IsResident(AssetID ID) const;
        NODISCARD TextureInfo GetInfo(TextureHandle Handle) const;
        NODISCARD size_t GetResidentCount() const { return _Entries.size(); }

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
        static std::vector<u8> DecodeImage(const u8* Bytes, size_t Size, TextureInfo& OutInfo);
        Entry CreateEntry(AssetID ID, u32 InitialRefCount);
        void FreeTexture(TextureHandle Handle) const;

        PAK::AssetRegistry* _Assets {nullptr};
        RHI::IRenderDevice* _Device {nullptr};
        Config _Config {};

        std::unordered_map<u32, std::vector<u8>> _PixelsByHandle;
        std::unordered_map<PAK::AssetIDValue, Entry> _Entries;
        std::unordered_map<u32, TextureInfo> _InfoByHandle;
        u32 _NextHandleID {1};  // 0 reserved for "invalid"
    };
}  // namespace Xen
