//
// Created by Jake Rieger on 9/7/2026.
//

#pragma once

#include "AssetID.hpp"
#include "Crypto.hpp"

#include <Common/Exception.hpp>
#include <array>

namespace Xen::PAK {
    DEFINE_ENGINE_EXCEPTION(InvalidPakException);

    constexpr std::array PAK_MAGIC   = {'X', 'P', 'A', 'K'};
    constexpr u32 PAK_FORMAT_VERSION = 3;  // v2 added encryption salt, v3 made encryption optional per-pak

    enum class PakCodec : u16 {
        None = 0,
        ZStd = 1,
        Lz4  = 2,
    };

    enum class PakFlags : u16 {
        None       = 0,
        Streamable = 1 << 0,
    };

    struct PakHeader {
        std::array<char, 4> Magic = PAK_MAGIC;
        u32 FormatVersion         = PAK_FORMAT_VERSION;
        u64 TableOffset           = 0;
        u32 TableEntryCount       = 0;

        /// @brief Whether every asset's stored bytes are AES-256-CTR
        /// encrypted (see PAKTool's --encrypt) - a dev pak built without it
        /// packs (and loads) noticeably faster, at the cost of the content
        /// being readable straight out of the file. Salt/KeyCheck below are
        /// meaningless (left zeroed) when this is false; PakFileSource skips
        /// both the key check and the decrypt step entirely.
        bool Encrypted = true;

        /// @brief Per-pak random salt used to derive each asset's CTR none.
        /// Zeroed and unused when Encrypted is false.
        PakSalt Salt {};

        /// @brief Known constant encrypted under the key. Used for detecting invalid key at
        /// read-time. Zeroed and unused when Encrypted is false.
        PakKeyCheck KeyCheck {};

        void Write(std::ostream& Out) const;
        static void WriteEmpty(std::ostream& Out);

        /// @brief Attempts to read a pak file header.
        /// @throws InvalidPakException On bad magic or unsupported version
        static PakHeader Read(std::istream& In);
    };

    struct PakTableEntry {
        AssetIDValue ID      = 0;
        u64 Offset           = 0;
        u32 UncompressedSize = 0;
        u32 CompressedSize   = 0;
        PakCodec Codec       = PakCodec::None;
        PakFlags Flags       = PakFlags::None;

        void Write(std::ostream& Out) const;
        static PakTableEntry Read(std::istream& In);
    };
}  // namespace Xen::PAK